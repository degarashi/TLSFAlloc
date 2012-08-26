#pragma once
#include "common.h"

namespace rs {
	// メモリアロケータインタフェース
	struct ImplTLSF {
		virtual ~ImplTLSF() {}
		virtual void* acquire(size_t s) = 0;
		virtual void* reacquire(void* p, size_t s) = 0;
		virtual void release(void* p) = 0;
		virtual size_t getRemainMem() const = 0;
		virtual size_t getSegmentSize(void* p) const = 0;
		virtual size_t LowFLevelSize() const = 0;
		virtual size_t LowBlockSize() const = 0;
		virtual void destroy() = 0;
	};
	#ifdef MSVC
		#pragma pack(push,1)
	#else
		#pragma pack(1)
	#endif
	template <class TSize, class THead, int MINSIZE>
	class MBlock {
		private:
			u8		_bUse;
			THead	_head;
			TSize	_szBlock;

			void _writeTail() {
				intptr_t pTail = ((intptr_t)this) + sizeof(*this) + _szBlock;
				*((TSize*)pTail) = getBlockSize();
			}

		public:
			// メモリブロックHead/Tailダミー用
			MBlock(bool bTail): _bUse(1), _szBlock(0) {
				if(bTail)
					_writeTail();
			}
			// 通常
			MBlock(TSize s, const THead& head): _bUse(0), _head(head), _szBlock(s-GetHeaderSize()) {
				_writeTail();
			}

			MBlock* next() {
				return (MBlock*)((intptr_t)this + getBlockSize());
			}
			MBlock* prev() {
				TSize szPB = *(TSize*)((intptr_t)this - sizeof(TSize));
				return (MBlock*)((intptr_t)this - szPB);
			}

			static size_t GetBlockSize(size_t s) {
				return GetHeaderSize() + s;
			}
			static size_t GetHeaderSize() {
				return sizeof(MBlock) + sizeof(TSize);
			}
			size_t getBlockSize() const {
				return GetBlockSize(_szBlock);
			}
			size_t getPayloadSize() const {
				return _szBlock;
			}

			THead* header() {
				return &_head;
			}

			// 前のブロックと結合
			void combinePrev() {
				MBlock* blk = prev();
				L_ASSERT(!blk->isUsing(), u8"使用中ブロックと結合を試みた");
				blk->_szBlock += getBlockSize();
				blk->_writeTail();
			}
			bool canCombinePrev() {
				return !prev()->isUsing();
			}
			// 後のブロックと結合
			void combineNext() {
				MBlock* blk = next();
				L_ASSERT(!blk->isUsing(), u8"使用中ブロックと結合を試みた");
				_szBlock += blk->getBlockSize();
				_writeTail();
			}
			bool canCombineNext() {
				return !next()->isUsing();
			}
			// 空きメモリを統合
			// (前ブロックのヘッダは別途修正する)
			MBlock* appendPrevMem(size_t s) {
				L_ASSERT(!isUsing(), u8"使用中ブロックに対して不正な操作");
				// ヘッダを前へずらし，サイズ変更，Tailを更新
				MBlock* blk = (MBlock*)((intptr_t)this - s);
				memmove(blk, this, sizeof(MBlock));

				blk->_szBlock += s;
				blk->_writeTail();
				return blk;
			}
			// ペイロードサイズを変更
			void adjustPayloadSize(size_t s) {
				// サイズ変更，Tailを更新
				_szBlock = s;
				_writeTail();
			}

			// ブロックを分割
			// 容量s + header + αがあれば切り分け(初期化はしない)
			size_t divide(size_t s, void** np) {
				size_t szNeed = GetHeaderSize()+MINSIZE;
				if(_szBlock >= szNeed+s) {
					size_t nsz = _szBlock - s;
					// ブロック縮小
					_szBlock -= nsz;
					_writeTail();
					// 新しくブロックが配置される場所を返す
					*np = (void*)((intptr_t)this + getBlockSize());
					return nsz;
				}
				return 0;
			}

			void* useThis(bool bUse) {
				L_ASSERT(!isUsing(), u8"");

				_bUse = bUse ? 1 : 0;
				_head.useThis();
				return payload();
			}
			void* payload() {
				return (void*)((intptr_t)this + sizeof(*this));
			}
			bool isUsing() const {
				L_ASSERT(_bUse==0 || _bUse==1, u8"不正な使用中フラグ");
				return _bUse==1;
			}
	};
	// ブロックサイズを格納するのに必要な型を決定
	template <int N, int SUM=0>
	struct GetNBit {
		enum {result=GetNBit< (N>>1), SUM+1 >::result};
	};
	template <int SUM>
	struct GetNBit<0, SUM> {
		enum {result=SUM};
	};

	// 2のべき乗分割 = NBit0
	// 等分割 = NBit1
	template <int NMemBit, int NBit0, int NBit1, bool BExc>
	class TLSF : public ImplTLSF {
		typedef CType<u8,
				CType<u16,
				CType<u32,
				CType<u32> > > >	CTLen;
		typedef typename TypeAt<CTLen, ((NMemBit-1)>>3) >::result TSize;

		private:
			const static int NDiv0 = 1<<NBit0,
							NDiv1 = 1<<NBit1,
							L1MASK = NDiv1-1,
							L1MASKINV = ~L1MASK,
							_LowFLevelSize = 1 << (NMemBit-NDiv0+1),
							_LowBlockSize = _LowFLevelSize >> NBit1;
			struct TLSFHead {
				typedef MBlock<TSize,TLSFHead, _LowBlockSize> MBlk;
				u8	bidx;
				MBlk *pPrev, *pNext;

				TLSFHead(int b=0, MBlk* p=nullptr, MBlk* n=nullptr): bidx(b), pPrev(p), pNext(n) {}
				void insertPrev(MBlk* blk, MBlk* ths) {
					if(pPrev)
						pPrev->header()->pNext = blk;
					pPrev = blk;
					blk->header()->pNext = ths;
				}
				void useThis() {
					if(pPrev)
						pPrev->header()->pNext = pNext;
					if(pNext)
						pNext->header()->pPrev = pPrev;
				}
			};
			typedef MBlock<TSize, TLSFHead, _LowBlockSize>	MBlk;

			// 2 level table
			MBlk* _mbIndex[1<<(NBit0+NBit1)];
			// bit table
			// :level1
			uint32_t	_btL0;
			// :level2
			uint32_t _btL1[NDiv0];

			void* _src;
			size_t _sz_src;
			// 空きメモリカウンタ
			size_t	_sz_remain;

			struct BIndex {
				uint32_t	value;

				BIndex(uint32_t v): value(v) {}
				operator uint32_t() { return value; }
				uint32_t L0Bit() const { return value>>NBit1; }
				uint32_t L1Bit() const { return value&L1MASK; }
			};
			// メモリブロックフリーリストのインデックス
			BIndex _calcIndex(size_t s) {
				// First_Level
				int nFS = NMemBit-NDiv0+1;
				u32 tmp = s >> nFS;
				int fLv = tmp==0 ? 0 : (Bit::MSB_N(tmp)+1);
				LA_OUTRANGE(fLv<NDiv0, u8"");
				// Second_Level
				int nSS = nFS+std::max(0,fLv-1)-NBit1;
				size_t stmp = s >> nSS;
				int sLv = stmp & L1MASK;
				LA_OUTRANGE(sLv<NDiv1, u8"");
				return (fLv << NBit1) | sLv;
			}
			void _pushMB(void* ptr, size_t s) {
				auto bidx = _calcIndex(s - MBlk::GetHeaderSize());
				MBlk* nblk = new(ptr) MBlk(s, TLSFHead(bidx));
				MBlk* blk = _mbIndex[bidx];
				if(blk)
					blk->header()->insertPrev(nblk, blk);
				_mbIndex[bidx] = nblk;

				// ビットフィールド編集
				_addFlag(bidx);

				_sz_remain += nblk->getPayloadSize();
			}
			void _addFlag(BIndex bidx) {
				_btL0 |= 1 << bidx.L0Bit();
				_btL1[bidx.L0Bit()] |= 1 << bidx.L1Bit();
				LA_OUTRANGE(_btL0 <(1<<NDiv0), u8"");
			}
			void _dropFlag(BIndex bidx) {
				_btL1[bidx.L0Bit()] &= ~(1 << bidx.L1Bit());
				if(_btL1[bidx.L0Bit()] == 0)
					_btL0 &= ~(1 << bidx.L0Bit());
				LA_OUTRANGE(_btL0 < (1<<NDiv0), u8"");
			}
			void* _useMB(BIndex bidx) {
				// 先頭ブロックを使用
				MBlk* blk = _mbIndex[bidx];
				L_ASSERT(blk, u8"");
				return _remBlock(blk, true);
			}
			void* _remBlock(MBlk* blk, bool flg) {
				BIndex bidx(blk->header()->bidx);
				if(_mbIndex[bidx] == blk) {
					if(!(_mbIndex[bidx] = blk->header()->pNext)) {
						// ビットフラグを落とす
						_dropFlag(bidx);
					}
				}
				_sz_remain -= blk->getPayloadSize();
				return blk->useThis(flg);
			}

			// 必要分だけとって残りは戻す
			void* _useDivMB(BIndex bidx, size_t s) {
				return _useDivMB(_ptrToBlock(_useMB(bidx)), s);
			}
			void* _useDivMB(MBlk* blk, size_t s) {
				L_ASSERT(blk->getPayloadSize() >= s, u8"");
				// 分割
				void* np;
				size_t nsz = blk->divide(s, &np);
				if(nsz > 0) {
					blk->header()->bidx = _calcIndex(s);
					_pushMB(np, nsz);
				}
				return blk->payload();
			}
		public:
			static MBlk* _ptrToBlock(void* p) {
				return reinterpret_cast<MBlk*>((intptr_t)p - sizeof(MBlk));
			}

		public:
			static size_t GetPaddingSize() {
				return MBlk::GetHeaderSize() + sizeof(u8);
			}
			size_t LowFLevelSize() const {
				return _LowFLevelSize;
			}
			size_t LowBlockSize() const {
				return _LowBlockSize;
			}

			// ソースメモリはNMemBitの容量を与える
			TLSF(void* src, size_t sz) {
				_src = src;
				_sz_src = sz;

				// preBlock (サイズ0)
				new(src) MBlk(true);
				// tailBlock (フラグオンリー)
				*((u8*)((intptr_t)src + sz - sizeof(u8))) = 1;

				// 実際に使えるメモリサイズ
				size_t r_sz = sz - MBlk::GetHeaderSize() - sizeof(u8);
				void* r_src = (void*)((intptr_t)src + MBlk::GetHeaderSize());

				// MBlockインデックスの初期化
				memset(_mbIndex, 0, sizeof(_mbIndex));
				// :BitTable L0,L1
				_btL0 = 0;
				for(int i=0 ; i<NDiv0 ; i++)
					_btL1[i] = 0;

				// 最初のブロックを追加
				_sz_remain = 0;
				_pushMB(r_src, r_sz);
			}
			virtual void destroy() {
				delete this;
			}
			// 確保メモリサイズの変更
			void* reacquire(void* p, size_t s) {
				s = std::max(s, LowBlockSize());

				// 現在のサイズより小さいか？
				MBlk* blk = _ptrToBlock(p);
				MBlk* nblk = blk->next();

				size_t cur_s = blk->getPayloadSize();
				bool bNUse = nblk->isUsing();		// 後続のブロックが空きか？
				if(cur_s >= s) {
					size_t pls_s = cur_s - s;
					if(pls_s >= LowBlockSize()+MBlk::GetHeaderSize()) {
						if(!bNUse) {
							// NBを一旦フリーリストから外す
							_remBlock(nblk, false);
							// 現ブロックサイズを調整
							blk->adjustPayloadSize(s);
							blk->header()->bidx = _calcIndex(s);
							// 空いた分を後続ブロックに加える
							nblk = nblk->appendPrevMem(pls_s);
							// NBを改めてフリーリストへ加える
							_pushMB(nblk, nblk->getBlockSize());
						} else {
							// 新たに空きブロックを作成する
							if(pls_s >= MBlk::GetHeaderSize() + LowBlockSize()) {
								// 現ブロックサイズを調整
								blk->adjustPayloadSize(s);
								blk->header()->bidx = _calcIndex(s);
								// 空きブロックを追加
								_pushMB((void*)((intptr_t)nblk - pls_s), pls_s);

								// 残量はヘッダを除いた分増やす
								//_sz_remain += pls_s - MBlk::GetHeaderSize();
							} else {
								// ヘッダ + DIVIDE_THRESHOLD分が確保出来なければ何もしない
								// 何も操作をしていないので残量は変化しない
							}
						}
					}
				} else {
					if(!bNUse) {
						// 後続のブロックを合わせたら要求サイズを満たせるか？
						// (後続ブロック + ヘッダサイズ分が空く)
						size_t pls_s = nblk->getBlockSize();
						if(cur_s+pls_s >= s) {
							// フリーリストから外す
							_remBlock(nblk, false);
							// ブロックを結合
							blk->combineNext();
							_useDivMB(blk, s);

							blk->header()->bidx = _calcIndex(blk->getPayloadSize());
	#ifdef TLSF_MEMFILL
							memset((void*)((intptr_t)p + cur_s), 0xca, blk->getPayloadSize()-cur_s);
							check();
	#endif
							return p;
						}
					}

					// 新しくブロックを確保してコピー
					void* np = acquire(s);
					// もし新しく領域を確保できなかったらnullを返す
					if(!np)
						return nullptr;

					memcpy(np, p, std::min(cur_s,s));
					release(p);
					return np;
				}
				return p;
			}

			void* acquire(size_t s) {
				s = std::max(s, LowBlockSize());
				if(s > getRemainMem()) {
					if(BExc)
						throw std::bad_alloc();
					return nullptr;
				}

				BIndex bidx = _calcIndex(s)+1;
				void* ret = nullptr;
				MBlk* blk = _mbIndex[bidx];
				// フリーリストがあるか？
				if(blk)
					ret = _useMB(bidx);
				else {
					// L1探索
					uint32_t bt = _btL1[bidx.L0Bit()];
					L_ASSERT(bidx.L0Bit()<NDiv0, u8"");
					// 容量以上のフラグが立っていればそれを使う
					uint32_t t_mask = ~((1<<bidx.L1Bit()) - 1);
					bt &= t_mask;
					if(bt != 0) {
						// L1に空きが見つかった
						BIndex nbI = (bidx.value&L1MASKINV) + Bit::MSB_N(bt);
						ret = _useDivMB(nbI, s);
					} else {
						// L0探索
						// 容量未満のフラグをマスク (L1には無かった結果なので1ビットずらす)
						t_mask = ~((2<<bidx.L0Bit())-1);
						bt = _btL0 & t_mask;
						if(bt != 0) {
							// L0に空きが見つかった
							int nbL0 = Bit::MSB_N(bt);
							// L1探索
							int nbL1 = Bit::MSB_N(_btL1[nbL0]);
							int nbI = (nbL0<<NBit1) | nbL1;
							ret = _useDivMB(nbI, s);
						}
						if(!ret) {
							if(BExc)
								throw std::bad_alloc();
							return ret;
						}
					}
				}
	#ifdef TLSF_MEMFILL
				memset(ret, 0xac, s);
	#endif
				return ret;
			}
			void check() {
				intptr_t endP = (intptr_t)_src + _sz_src - 1;
				MBlk* blk = (MBlk*)_src;
				// 全てのブロックを巡回する
				while((intptr_t)blk != endP) {
					// BIndexのフリーリストにきちんと入っているか
					u32 bidx = blk->header()->bidx;
					if(_calcIndex(blk->getPayloadSize()) != bidx)
						__asm__("int 3");

					if(!blk->isUsing()) {
						bool bFound = false;
						MBlk* tblk = _mbIndex[bidx];
						while(tblk) {
							if(tblk == blk) {
								bFound = true;
								break;
							}
							tblk = tblk->header()->pNext;
						}
						if(!bFound)
							__asm__("int 3");

						if((intptr_t)_src!=(intptr_t)blk &&
							!_mbIndex[bidx])
							__asm__("int 3");
					}
					blk = blk->next();
				}
			}

			void release(void* ptr) {
				MBlk* blk = _ptrToBlock(ptr);
				L_ASSERT(blk->isUsing(), u8"管轄外メモリが渡された");
	#ifdef TLSF_MEMFILL
				memset(ptr, 0xfc, blk->getPayloadSize());
	#endif
				// 前後のブロックと結合を試みる
				if(blk->canCombinePrev()) {
					MBlk* bptr = blk->prev();
					_remBlock(bptr, false);
					blk->combinePrev();

					bptr->header()->bidx = _calcIndex(bptr->getPayloadSize());
					blk = bptr;
				}
				if(blk->canCombineNext()) {
					_remBlock(blk->next(), false);
					blk->combineNext();

					blk->header()->bidx = _calcIndex(blk->getPayloadSize());
				}

				_pushMB(blk, blk->getBlockSize());
			}
			size_t getRemainMem() const {
				return _sz_remain;
			}
			size_t getSegmentSize(void* p) const {
				MBlk* blk = _ptrToBlock(p);
				return blk->getPayloadSize();
			}

			// ランダムなサイズで確保，解放，サイズ変更を繰り返しテスト
			void unit_test(int n) {
				const int N_ITER = 256;
				const int modsize = _sz_src / (N_ITER*2);
				for(int i=0 ; i<n ; i++) {
					void* ptr[N_ITER];
					int idx[N_ITER];

					// :shuffle index
					for(int j=0 ; j<256 ; j++)
						idx[j] = 255-j;
					for(int j=0 ; j<256-1 ; j++)
						std::swap(idx[j], idx[(rand()%(256-1))+1]);

					// :acquire
					for(int j=0 ; j<256 ; j++)
						ptr[j] = acquire(rand()%modsize);
					check();
					// :resize check
					for(int j=0 ; j<256 ; j++)
						ptr[idx[j]] = reacquire(ptr[idx[j]], rand()%modsize);
					check();
					// :release
					for(int j=0 ; j<256 ; j++)
						release(ptr[idx[j]]);
					check();
				}
			}
			uintptr_t getEndPtr() const {
				return (uintptr_t)_src + _sz_src;
			}
	};
	#ifdef MSVC
		#pragma pack(pop)
	#else
		#pragma pack(0)
	#endif

	// 通常のnew,deleteを使用
	class TLSFDefault : public ImplTLSF {
		public:
			void* acquire(size_t s) { return malloc(s); }
			void* reacquire(void* p, size_t s) { return realloc(p, s); }
			void release(void* p) { free(p); }
			size_t getRemainMem() const { return (size_t)std::numeric_limits<size_t>::max; }
			size_t getSegmentSize(void* p) const { return 0; }
			size_t LowFLevelSize() const { return 0; }
			size_t LowBlockSize() const { return 0; }
			virtual void destroy() { delete this; }
	};
	// 指定されたメモリ領域(最大1<<NMemBit-1)を内部でnew確保，解放
	template <int NMemBit, int NBit0, int NBit1, bool BExc>
	class TLSFNew : public TLSF<NMemBit, NBit0, NBit1, BExc> {
		typedef TLSF<NMemBit, NBit0, NBit1, BExc> _TLSF;
		private:
			u8*	_pBuff;
		public:
			const static size_t MAXSIZE = (1<<NMemBit)-1;

			TLSFNew(size_t sz=size_t(1<<NMemBit)-1):
				_TLSF(_pBuff=new u8[std::min(sz,size_t(1<<NMemBit)-1)], std::min(sz,size_t(1<<NMemBit)-1)) {}
			virtual void destroy() {
				delete[] _pBuff;
				_TLSF::destroy();
			}
	};

	// 複数の内部TLSFアロケータを持ち，必要に応じて一定量ずつ追加でメモリ領域を確保
	// (今の所解放はしない)
	template <int NMemBit, int NBit0, int NBit1>
	class TLSFBlock : public ImplTLSF {
		private:
			const static size_t MAXSIZE = (1<<NMemBit)-1;
			typedef TLSFNew<NMemBit,NBit0,NBit1,false>	_TLSF;

			const size_t	_szBlock;
			_TLSF*			_top;
			// TLSFアロケータリスト(トップのクラス内に確保)
			typedef std::pair<uintptr_t, _TLSF*>	TLSPair;	// ポインタ範囲(End)
			TLSPair*		_alcList;
			int				_nAlc, _szAlc;

			_TLSF* _addNewBlock() {
				// アロケータリストが足りるか
				if(_szAlc-1 == _nAlc) {
					// 2倍に拡張
					_szAlc *= 2;
					_top->reacquire(_alcList, sizeof(TLSPair)*_szAlc);
				}
				_TLSF* m = new _TLSF(_szBlock);
				_alcList[_nAlc++] = TLSPair(m->getEndPtr(), m);
				return m;
			}
			int _witchMem(void* p) const {
				for(int i=0 ; i<_nAlc-1 ; i++)
					if((uintptr_t)p < _alcList[i].first)
						return i;
				return _nAlc-1;
			}
		public:
			// 追加ブロック容量を設定
			TLSFBlock(size_t sz=MAXSIZE): _szBlock(sz), _top(new _TLSF(sz)) {
				_alcList = (TLSPair*)_top->acquire(sizeof(TLSPair)*4);
				memset(_alcList, 0, sizeof(TLSPair)*4);
				_alcList[0] = TLSPair(_top->getEndPtr(), _top);
				_szAlc = 4;
				_nAlc = 1;
			}
			virtual void destroy() {
				// (topのTLSFはリストを含んでいる為，最後にする)
				for(int i=1 ; i<_nAlc ; i++)
					delete _alcList[i].second;
				_top->release(_alcList);
				delete _top;
				delete this;
			}

			 void* acquire(size_t s) {
				if(_szBlock-_TLSF::GetPaddingSize() < s)
					throw std::bad_alloc();
				void* ret;
				// リストの上から順番に確保を試みる
				for(int i=0 ; i<_nAlc ; i++) {
					if(ret = _alcList[i].second->acquire(s))
						return ret;
				}
				// 新しいブロックを追加
				_TLSF* tls = _addNewBlock();
				return tls->acquire(s);
			}
			void release(void* p) {
				// 範囲チェックによりどのクラスの物か特定
				_alcList[_witchMem(p)].second->release(p);
			}
			void* reacquire(void* p, size_t s) {
				// サイズが大きくなる場合，同じアロケータでは確保できない可能性がある
				auto* pTls = _alcList[_witchMem(p)].second;
				void* ret = pTls->reacquire(p, s);
				if(!ret) {
					// 別アロケータから確保し，コピー
					ret = acquire(s);
					auto* blk = pTls->_ptrToBlock(p);
					memcpy(ret, blk->payload(), blk->getPayloadSize());
					release(p);
				}
				return ret;
			}
			size_t getRemainMem() const {
				size_t count = 0;
				for(int i=0 ; i<_nAlc ; i++)
					count += _alcList[i].second->getRemainMem();
				return count;
			}
			size_t getSegmentSize(void* p) const {
				return _alcList[_witchMem(p)].second->getSegmentSize(p);
			}
			size_t LowFLevelSize() const {
				return _top->LowFLevelSize();
			}
			size_t LowBlockSize() const {
				return _top->LowBlockSize();
			}
	};
}