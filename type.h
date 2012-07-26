/* テンプレート使用のタイプリスト */
#pragma once

// タイプリスト
struct NullType  { enum { VALID = 0 }; };
template <class T0, class T1=NullType>
struct CType {
	typedef T0 type;
	typedef T1 next;
	enum { ISPAIR = 0, VALID = 1 };
};
template <class T0, class T1=NullType>
struct CPair {
	typedef T0 type;
	typedef T1 next;
	enum { ISPAIR = 1, VALID = 1 };
};

// タイプリストから，ある型の先頭からの位置を求める
template <class CBASE, class CT>
struct TypePos;
template <class T0, class T1, class T2>
struct TypePos< CType< CType<T0, T1>, T2>, CType<T0, T1> > {
	enum { result=0 };
};
template <class T0, class T1, class T2>
struct TypePos< CType< CPair<T0, T1>, T2>, CPair<T0, T1> > {
	enum { result=0 };
};

template <class T0, class T2>
struct TypePos< CType< T0, T2>, T0 > {
	enum { result=0 };
};

template <class T0, class T1, class CT>
struct TypePos< CType<T0, T1>, CT > {
	enum { result=TypePos<T1, CT>::result + 1 };
};
template <class T0, class T1, class CT>
struct TypePos< CPair<T0, T1>, CT > {
	enum { result=TypePos<T1, CT>::result + 1 };
};

// タイプリストに目当ての型が含まれるか
template <class CBASE, class CT>
struct HasType;
template <class T0, class T1, class CT>
struct HasType<CType<T0,T1>, CT> {
	enum { has=HasType<T1,CT>::has };
};
template <class T1, class CT>
struct HasType< CType<CT,T1>, CT> {
	enum { has=1 };
};
template <class CT>
struct HasType<NullType, CT> {
	enum { has=0 };
};

// タイプリストの長さを求める
template <class T0>
struct TypeLength;
template <class T0, class T1>
struct TypeLength< CType<T0, T1> > {
	enum { length = TypeLength<T1>::length + 1 };
};
template <>
struct TypeLength<NullType> {
	enum { length = 0 };
};


// テンプレートMax
template <int N0, int N1>
struct IntMax {
	enum { result = IntMax<N0-1, N1-1>::result + 1 };
};
template <int N0>
struct IntMax<N0, 0> {
	enum { result = N0 };
};
template <int N1>
struct IntMax<0, N1> {
	enum { result = N1 };
};
template <>
struct IntMax<0, 0> {
	enum { result = 0 };
};

// テンプレートMin
template <int N0, int N1>
struct IntMin {
	enum { result = IntMin<N0-1, N1-1>::result + 1 };
};
template <int N0>
struct IntMin<N0, 0> {
	enum { result = 0 };
};
template <int N1>
struct IntMin<0, N1> {
	enum { result = 0 };
};
template <>
struct IntMin<0, 0> {
	enum { result = 0 };
};

// タイプリストから１つピックアップ
template <class T0, int N>
struct TypeAt;
template <class T0, class T1>
struct TypeAt<CType<T0, T1>, 0> {
	typedef T0	result;
};

template <class T0, class T1, int N>
struct TypeAt<CType<T0, T1>, N> {
	typedef typename TypeAt<T1, 
			IntMin<N, TypeLength< CType<T0, T1> >::length-1>::result-1
			>::result	result;
};

// 定数Nの使用ビット
template <int N>
struct BitLength {
	enum { length = BitLength<(N>>1)>::length + 1 };
};
template <>
struct BitLength<0> {
	enum { length = 0 };
};

// 型変換判定
template <class T, class U>
class Conversion {
	public:
		typedef char Small;
		class Big { char d[2]; };
		static Small Test(U);
		static Big Test(...);
		static T MakeT();

	public:
		enum { exists =
			sizeof(Test(MakeT())) == sizeof(Small),
			sameType = 0};
};
template <class T>
class Conversion<T,T> {
	public:
		enum { exists=1, sameType=1 };
};
template <class T>
class Conversion<void, T> {
	public:
		enum { exists=0, sameType=0 };
};
template <class T>
class Conversion<T, void> {
	public:
		enum { exists=0, sameType=0 };
};
template <>
class Conversion<void, void> {
	public:
		enum { exists=1, sameType=1 };
};
#define ISDERIVED(T,U) (Conversion<const U*, const T*>::exists && \
	!Conversion<const T*, const void*>::sameType)

// constや参照を取り除いた生の型を取得
template <class T> struct _RawType { typedef T result; };
template <class T> struct _RawType<const T> { typedef T result; };
template <class T> struct RawType { typedef typename _RawType<T>::result result; };
template <class T> struct RawType<T*> { typedef typename _RawType<T>::result result; };
template <class T> struct RawType<T&> { typedef typename _RawType<T>::result result; };
template <class T> struct RawType<T&&> { typedef typename _RawType<T>::result result; };