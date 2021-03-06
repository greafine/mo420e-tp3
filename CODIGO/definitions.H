#ifndef __DEFINITIONS_H__
#define __DEFINITIONS_H__

#define MAX_CPU_TIME 1800
#define MAX_NUM_CORTES 100   /* limite maximo de cortes */
#define MAX_ITER_SEP 3   /* numero maximo de iteracoes de corte por nodo */

#define EPSILON 1e-6
#define INF 1e14

#define pb push_back
#define sz(a) (int) (a).size()
#define NONZERO(a) ((a) > EPSILON || (a) < -EPSILON)
#define present(c,x) ((c).find(x) != (c).end()) 
#define traverse(c,i) for(typeof((c).begin()) i = (c).begin(); i != (c).end(); i++) 
#define all(a) (a).begin(),(a).end()

#include <algorithm>

typedef std::pair<int,int> ii;

inline int ijtoe(int i, int j, int n) {
  int e = 0;
  for(int k=0;k<i;k++) e += n-1-k;
  e += j-i-1;
  return(e);
}
inline ii etoij(int e, int n) {
  int i = 0, j;
  for(int k=0;e >= 0;k++) {
    i = k;
    e -= n-k-1;
  }
  j = e + n;
  return(std::make_pair(i,j));
}


#endif
