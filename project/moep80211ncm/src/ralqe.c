#include <stdio.h>
#include <time.h>
#include <math.h>

#include <immintrin.h>

#include <moepcommon/util.h>

#include "global.h"
#include "ralqe.h"


static double
numpackets_stable(int p, int q, double thresh)
{
	int i,j;
	double sum, p1, pqj1;
	double s[GENERATION_SIZE];

	sum = 0;
	for (i=0; i<GENERATION_SIZE; i++)
		s[i] = 1.0;

	sum = 1.0;
	for (j=1; (1.0-sum)<thresh; j++) {
		sum = 0.0;
		pqj1 = p+q+j+1;
		p1 = (double)(p+j)/pqj1;
		for (i=0; i<GENERATION_SIZE; i++) {
			if (j<=i)
				s[i] *= p1;
			else
				s[i] *= (double)(q+j-i)*(double)(j)/((double)(j-i)*pqj1);

			sum += s[i];
		}
	}

	return (double)j/GENERATION_SIZE;
}


//static double
//numpackets_stable(int p, int q, double thresh)
//{
//	double s[GENERATION_SIZE], sr[4], *sp;
//	float _pqj1[8];
//	float _pqj1j[8];
//	float _jqjj[8];
//	float _p1[8];
//	int _j, k;
//
//	__m256d s1, s2, acc;
//	__m256 p1, p2, i, j, pqj1, pqj1j, jqjj, tmp, i2;
//	__m128 low;
//
//	__m256 ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
//
//	const __m256 c8 = _mm256_set1_ps(8);
//
//	for (sp=s; sp<&s[GENERATION_SIZE]; sp+=4)
//		_mm256_store_pd(sp, _mm256_set1_pd(1.0));
//
//	sr[0] = 1.0;
//	for (_j=1; (1.0-sr[0]) < thresh; _j++) {
//		acc = _mm256_setzero_pd();
//		i = _mm256_set_ps(7,6,5,4,3,2,1,0);
//
////		p1    = _mm256_set1_ps((float)(p+_j)/(float)(p+q+_j+1));
////
////		j     = _mm256_set1_ps(_j);
////		pqj1  = _mm256_set1_ps(p+q+_j+1);
////		jqjj  = _mm256_set1_ps(_j*(_j+q));
////		pqj1j = _mm256_set1_ps((p+q+_j+1)*_j);
//
//		k = (_j-1) % 8;
//		if (k == 0) {
//			ymm0 = _mm256_set_ps(_j+7,_j+6,_j+5,_j+4,_j+3,_j+2,_j+1,_j);
//			ymm1 = _mm256_add_ps(ymm0, _mm256_set1_ps(p));	// p+j
//			ymm2 = _mm256_add_ps(ymm0, _mm256_set1_ps(p+q+1));	// p+q+1+j
//			ymm3 = _mm256_mul_ps(ymm0, ymm2);	// j*(p+q+j+1)
//			ymm4 = _mm256_set1_ps(q);		// j*(j+q)
//			ymm4 = _mm256_add_ps(ymm4,ymm0);	// j*(j+q)
//			ymm4 = _mm256_mul_ps(ymm4,ymm0);	// j*(j+q)
//			ymm5 = _mm256_div_ps(ymm1,ymm2);
//
//			_mm256_store_ps(_pqj1, ymm2);
//			_mm256_store_ps(_pqj1j, ymm3);
//			_mm256_store_ps(_jqjj, ymm4);
//			_mm256_store_ps(_p1, ymm5);
//		}
//
//		p1    = _mm256_set1_ps(_p1[k]);
//		j     = _mm256_set1_ps(_j);
//		pqj1  = _mm256_set1_ps(_pqj1[k]);
//		jqjj  = _mm256_set1_ps(_jqjj[k]);
//		pqj1j = _mm256_set1_ps(_pqj1j[k]);
//
//		for (sp=s; sp<s+GENERATION_SIZE; sp+=8) {
//			tmp = _mm256_fmsub_ps(i, pqj1, pqj1j);
//			p2 = _mm256_fmsub_ps(i, j, jqjj);
//			tmp = _mm256_rcp_ps(tmp);
//
//			s1 = _mm256_load_pd(sp);
//			s2 = _mm256_load_pd(sp+4);
//
//			p2 = _mm256_mul_ps(p2, tmp);
//
//			tmp = _mm256_cmp_ps(j, i, _CMP_LE_OS);
//
//			i = _mm256_add_ps(i, c8);
//
//			tmp = _mm256_blendv_ps(p2, p1, tmp);
//
//			low = _mm256_extractf128_ps(tmp, 0);
//			s1 = _mm256_mul_pd(s1, _mm256_cvtps_pd(low));
//			_mm256_store_pd(sp, s1);
//			acc = _mm256_add_pd(acc, s1);
//
//			low = _mm256_extractf128_ps(tmp, 1);
//			s2 = _mm256_mul_pd(s2, _mm256_cvtps_pd(low));
//			_mm256_store_pd(sp+4, s2);
//			acc = _mm256_add_pd(acc, s2);
//		}
//
//		acc = _mm256_hadd_pd(acc, acc);
//		acc = _mm256_permute4x64_pd(acc, 0xe8);
//		acc = _mm256_hadd_pd(acc, acc);
//
//		_mm256_store_pd(sr, acc);
//	}
//
//	return (float)_j/GENERATION_SIZE;
//}
//
//static double
//binommean(int p, int q, double t)
//{
//	int i,j;
//	int n;
//	double factor,sum;
//	double qual = (double)p/(double)(p+q);
//
//	sum = 1.0;
//	for (n=GENERATION_SIZE; 1.0-sum < t; n++) {
//		sum = 0.0;
//		for (i=0; i<GENERATION_SIZE; i++) {
//			factor = 1.0;
//			for (j=1; j<=i; j++) {
//				factor *= qual*(double)(n-j+1)/(double)j;
//			}
//			sum += pow(1.0-qual,n-i)*factor;
//		}
//	}
//
//	return (double)n/(double)GENERATION_SIZE;
//}


struct ralqe_link {
	double p, q;
	struct timespec t;
};


ralqe_link_t
ralqe_link_new()
{
	ralqe_link_t l;

	if (!(l = malloc(sizeof(*l))))
		return NULL;
	memset(l, 0, sizeof(*l));
	return l;
}

void
ralqe_link_del(ralqe_link_t l)
{
	free(l);
}

void
ralqe_init(ralqe_link_t l, int p, int q)
{
	clock_gettime(CLOCK_MONOTONIC, &l->t);
	l->p = p;
	l->q = q;
}

void
ralqe_update(ralqe_link_t l, int *p, int *q)
{
	struct timespec now;
	double t;

	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecsub(&now, &l->t);
	timespecadd(&l->t, &now);
	t = (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
	t = exp(RALQE_TAU * -t);
	l->p = l->p * t + *p;
	l->q = l->q * t + *q;
	t = l->p + l->q;
	if (t > RALQE_MAX) {
		l->p *= (double)RALQE_MAX/t;
		l->q *= (double)RALQE_MAX/t;
	}
	*p = l->p;
	*q = l->q;
}

double
ralqe_redundancy(ralqe_link_t l, double thresh)
{
	int p, q;

	p = 0;
	q = 0;
	ralqe_update(l, &p, &q);

	return numpackets_stable(l->p, l->q, thresh);
}
