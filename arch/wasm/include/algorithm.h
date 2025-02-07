#ifndef ALGORITHM_H
#define ALGORITHM_H

// copied from https://github.com/zouxiaohang/TinySTL/blob/master/TinySTL/Algorithm.h
template <class T>
const T& min(const T& a, const T& b){
	return !(b < a) ? a : b;
}
template <class T>
const T& max(const T& a, const T& b){
	return (a < b) ? b : a;
}

#endif