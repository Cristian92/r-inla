
/* tabulate-Qfunc.c
 * 
 * Copyright (C) 2004-2020 Havard Rue
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The author's contact information:
 *
 *        Haavard Rue
 *        CEMSE Division
 *        King Abdullah University of Science and Technology
 *        Thuwal 23955-6900, Saudi Arabia
 *        Email: haavard.rue@kaust.edu.sa
 *        Office: +966 (0)12 808 0640
 *
 */

/*!
  \file tabulate-Qfunc.c
  \brief Tabulate a Qfunction. Useful for speeding up costly Qfunctions.

  These functions provide a tool for tabulate a Qfunction to gain speeup. On partiuclar example are those computed with \c
  GMRFLib_init_wa_problem(), which provides a convenient way to compute Qfunctions, but can sometimes be slow if the elements,
  perhaps apart from a common prec (in real or log_prec), does not change. For these cases then \c GMRFLib_tabulate_Qfunc
  can provide a large speedup.

  The functions are
  - \c GMRFLib_tabulate_Qfunc()  tabulate a Qfunction and return a \c GMRFLib_tabulate_Qfunc_tp object
  - \c GMRFLib_free_tabulate_Qfunc()  free's a \c GMRFLib_tabulate_Qfunc_tp object

  Example:
  \verbatim

  double prec;
  GMRFLib_wa_problem_tp *wa_problem = NULL;
  GMRFLib_tabulate_Qfunc_tp *tab = NULL;
  
  GMRFLib_init_wa_problem(&wa_problem, wagraph, waQfunc, NULL); // init the wa-problem, here prec=1
  GMRFLib_tabulate_Qfunc(&tab, wa_problem->graph, wa_problem->Qfunc, wa_problem->Qfunc_arg, &prec,NULL);

  prec = 2.0;					  
  for(i=0;i<wa_problem->graph->n;i++)
  {
      for(jj=0;jj<wa_problem->graph->nnbs[i];jj++)
      {
          j = wa_problem->graph->nbs[i][jj];
	  printf("Qfunc(%d,%d) = (wa) %f (tabulated) %f\n", i, j,
	       (*wa_problem->Qfunc)(i, j, wa_problem->Qfunc_arg), (*tab->Qfunc)(i, j, tab->Qfunc_arg)/prec);
       }
   }

  \endverbatim
*/

#include <stdio.h>
#if !defined(__FreeBSD__)
#include <malloc.h>
#endif
#include <stdlib.h>

#ifndef GITCOMMIT
#define GITCOMMIT
#endif
static const char GitID[] = "file: " __FILE__ "  " GITCOMMIT;

/* Pre-hg-Id: $Id: tabulate-Qfunc.c,v 1.59 2009/12/15 12:26:03 hrue Exp $ */

#include "GMRFLib/GMRFLib.h"
#include "GMRFLib/GMRFLibP.h"

static unsigned char ADD_MULTIPLE_ENTRIES = 0;		       /* 1: allow, 0: no allow (abort...) */
#define PREVIOUS_VALUE (ADD_MULTIPLE_ENTRIES ? (prev ? *prev : 0.0) : 0.0)

#define CHECK_FOR_MULTIPLE_ENTRIES(table, index, value)			\
	prev = map_id_ptr(table, index);				\
	if (!ADD_MULTIPLE_ENTRIES) {					\
		if (prev && *prev) {					\
			fprintf(stderr, "\n\n%s:%1d: Override previous value in table %g with %g, please report this to <help@r-inla.org>\n\n", \
				__GMRFLib_FuncName, __LINE__, *prev, value); \
			assert(!(prev && *prev));			\
			abort();					\
		}							\
	}

#define TAB_FUNC_CORE(_prec_scale)					\
	GMRFLib_tabulate_Qfunc_arg_tp *args = NULL;			\
	double prec, *dp;						\
	int i, ii, j, len, imin, imax;					\
	args = (GMRFLib_tabulate_Qfunc_arg_tp *) arg;			\
	if (_prec_scale) {						\
		prec = GMRFLib_SET_PREC(args);				\
	}								\
	imin = IMIN(node, nnode);					\
	imax = IMAX(node, nnode);					\
	if (nnode >= 0) {						\
		if (args->Q) {						\
			int offset = args->Q->ia[imin];			\
			j = offset + GMRFLib_iwhich_sorted(imax, offset + args->Q->ja, args->Q->ia[imin + 1] - offset);	\
			assert(j >= offset);				\
			dp = &(args->Q->a[j]);				\
		} else if (args->Q_idx) {				\
			map_ii_get(args->Q_idx[imin], imax, &ii);	\
			dp = &(args->Q->a[ii]);				\
		} else {						\
			dp = map_id_ptr(args->values[imin], imax);	\
		}							\
		if (_prec_scale) {					\
				  return prec * (*dp);			\
		} else {						\
			return *dp;					\
		}							\
	} else {							\
		if (args->Q) {						\
			assert(args->Q->base == 0);			\
			j = args->Q->ia[node];				\
			len = args->Q->ia[node + 1] - j;		\
			memcpy(values, &(args->Q->a[j]), len * sizeof(double)); \
		} else {						\
			return NAN;					\
		}							\
		if (_prec_scale) {					\
			for (i = 0; i < len; i++) {			\
				values[i] *= prec;			\
			}						\
		}							\
		return 0.0;						\
	 }

double GMRFLib_tabulate_Qfunction(int node, int nnode, double *values, void *arg)
{
	TAB_FUNC_CORE(1);
}
double GMRFLib_tabulate_Qfunction_std(int node, int nnode, double *values, void *arg)
{
	TAB_FUNC_CORE(0);
}

/*!
  \brief Tabulate a Qfunction to gain speedup.

  \param[out] tabulate_Qfunc At output, \a tabulate_Qfunc is allocated as a pointer to a \c
  GMRFLib_tabulate_Qfunc_tp, holding a pointer to the Qfunction (GMRFLib_tabulate_Qfunc_tp::Qfunc)
  and its argument (GMRFLib_tabulate_Qfunc_tp::Qfunc_arg) which evaluate the tabulate Qfunction.

  \param[in] graph The graph

  \param[in] Qfunc  The Qfunction to be tabulate

  \param[in] Qfunc_arg The argument to the Qfunction to the tabulate

  \param[in] prec An optional argument which scales the Qfunction (the `precision'). If \c prec is not \c NULL, then
  GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc times *prec. If \c prec is \c NULL, then \c log_prec is tried.

  \param[in] log_prec An optional argument which scales the Qfunction. If \c log_prec is not \c NULL, then
  GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc times * exp(log_prec). If \c log_prec is \c NULL, then \c log_prec_omp is tried.

  \param[in] log_prec_omp An optional argument which precs the Qfunction. If \c log_prec_omp is not \c NULL, then GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc
  times * exp(log_prec_omp[GMRFLib_thread_id]).  If \c log_prec_omp is \c NULL, then prec is set to 1.0.

*/
int GMRFLib_tabulate_Qfunc(GMRFLib_tabulate_Qfunc_tp ** tabulate_Qfunc, GMRFLib_graph_tp * graph,
			   GMRFLib_Qfunc_tp * Qfunc, void *Qfunc_arg, double *prec, double *log_prec, double **log_prec_omp)
{

	int i, j, k;

	GMRFLib_tabulate_Qfunc_arg_tp *arg = NULL;

	*tabulate_Qfunc = Calloc(1, GMRFLib_tabulate_Qfunc_tp);
	arg = Calloc(1, GMRFLib_tabulate_Qfunc_arg_tp);
	(*tabulate_Qfunc)->Qfunc_arg = (void *) arg;

	arg->n = graph->n;
	arg->prec = prec;
	arg->log_prec = (prec == NULL ? log_prec : NULL);
	if (prec == NULL && log_prec == NULL && log_prec_omp != NULL) {
		int tmax = GMRFLib_MAX_THREADS;
		arg->log_prec_omp = Calloc(tmax, double *);
		for (i = 0; i < tmax; i++) {
			arg->log_prec_omp[i] = log_prec_omp[i];
		}
	} else {
		arg->log_prec_omp = NULL;
	}

	if (prec == NULL && log_prec == NULL && log_prec_omp == NULL) {
		(*tabulate_Qfunc)->Qfunc = GMRFLib_tabulate_Qfunction_std;
	} else {
		(*tabulate_Qfunc)->Qfunc = GMRFLib_tabulate_Qfunction;
	}

	// Currently do this for PARDISO only, was some issues to extend it to TAUCS, and that require some more
	// checking/rewrite.
	if (GMRFLib_smtp == GMRFLib_SMTP_PARDISO) {
		GMRFLib_Q2csr(&(arg->Q), graph, Qfunc, Qfunc_arg);
		if (!(arg->Q->a[0] > 0.0)) P(arg->Q->a[0]);
		assert(arg->Q->a[0] > 0.0);
		GMRFLib_graph_duplicate(&(arg->graph), graph);

		// I disable this for now, seems to take more time than it save? 
		if (0) {
			arg->Q_idx = Calloc(graph->n, map_ii *);
			int idx = 0;

			for (i = 0; i < graph->n; i++) {
				arg->Q_idx[i] = Calloc(1, map_ii);
				map_ii_init_hint(arg->Q_idx[i], graph->lnnbs[i] + 1);
				arg->Q_idx[i]->defaultvalue = -2;
				map_ii_set(arg->Q_idx[i], i, idx);
				idx++;

				for (j = 0; j < graph->lnnbs[i]; j++) {
					k = graph->lnbs[i][j];
					map_ii_set(arg->Q_idx[i], k, idx);
					idx++;
				}
			}
			assert(idx == arg->Q->na);
		}

	} else {
		int id, mem_id;

		id = GMRFLib_thread_id;
		mem_id = GMRFLib_meminfo_thread_id;
		arg->values = Calloc(graph->n, map_id *);

		omp_set_num_threads(GMRFLib_openmp->max_threads_inner);
#pragma omp parallel for private(i, j, k) num_threads(GMRFLib_openmp->max_threads_inner)
		for (i = 0; i < graph->n; i++) {
			GMRFLib_thread_id = id;
			GMRFLib_meminfo_thread_id = mem_id;
			arg->values[i] = Calloc(1, map_id);
			map_id_init_hint(arg->values[i], graph->lnnbs[i] + 1);
			map_id_set(arg->values[i], i, (*Qfunc) (i, i, NULL, Qfunc_arg));	/* diagonal */

			for (j = 0; j < graph->lnnbs[i]; j++) {
				k = graph->lnbs[i][j];
				map_id_set(arg->values[i], k, (*Qfunc) (i, k, NULL, Qfunc_arg));
			}
		}
		GMRFLib_thread_id = id;
		GMRFLib_meminfo_thread_id = mem_id;
	}

	return GMRFLib_SUCCESS;
}


/*!
  \brief Read a tabulated Qfunction from a file.

  This function reads a tabulated Qfunction from a ascii file, and setup a Qfunction and its arguments which return these elements. A further argument can scale
  each element, just like a precision. It also computes the corresponding graph. The indexing can be either zero or one-based. If one-based, then its converted to
  zero-based.

  \param[out] tabulate_Qfunc \a tabulate_Qfunc is a ** of type \a GMRFLib_tabulate_Qfunc_arg_tp, see
  the example.

  \param[out] graph The graph-object computed

  \param[in] filename The name of the file. The format of the file is ascii, consisting of triplets "i j Q(i,j)". Comment-lines starts with a "#".  If the same
  element (i,j,Qij) is given more than once, then the last element is used.

  \param[in] prec An optional argument which scales the Qfunction (the `precision'). If \c prec is not \c NULL, then
  GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc times *prec. If \c prec is \c NULL, then \c log_prec is tried.

  \param[in] log_prec An optional argument which scales the Qfunction. If \c log_prec is not \c NULL, then
  GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc times * exp(log_prec). If \c log_prec is \c NULL, then \c log_prec_omp is tried.

  \param[in] log_prec_omp An optional argument which precs the Qfunction. If \c log_prec_omp is not \c NULL, then
  GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc times * exp(log_prec_omp[GMRFLib_thread_id]).  If \c log_prec_omp is \c
  NULL, then prec is set to 1.0.


*/
int GMRFLib_tabulate_Qfunc_from_file(GMRFLib_tabulate_Qfunc_tp ** tabulate_Qfunc, GMRFLib_graph_tp ** graph, const char *filename,
				     int dim, double *prec, double *log_prec, double **log_prec_omp)
{
	/*
	 * as GMRFLib_tabulate_Qfunc(), but reads the Q_ij values from file with name FILENAME, in format
	 * 
	 * i j Q_{ij} : : : i j Q_{ij}
	 *
	 *
	 * only values i<=j are required. duplicated values are added up.
	 */

	int i, j, ii, jj, k, ntriples, err, debug = 0, imin = INT_MAX, jmin = INT_MAX, off = 0, sparse = 0;
	double value, *prev;

	GMRFLib_tabulate_Qfunc_arg_tp *arg = NULL;
	GMRFLib_io_tp *io = NULL;
	GMRFLib_error_handler_tp *old_handler;
	GMRFLib_matrix_tp *M = NULL;

	/*
	 * step 1. build the graph 
	 */

	GMRFLib_ged_tp *ged = NULL;

	GMRFLib_ged_init(&ged, NULL);

	/*
	 * to fix the dimension, possibly padding with zero's 
	 */
	GMRFLib_ged_add(ged, 0, 0);
	if (dim > 0) {
		GMRFLib_ged_add(ged, dim - 1, dim - 1);
	}

	if (GMRFLib_is_fmesher_file(filename, (long int) 0, -1) == GMRFLib_SUCCESS) {
		M = GMRFLib_read_fmesher_file(filename, (long int) 0, -1);
		sparse = (M->i && M->j);
		if (!sparse) {
			assert(M->ncol == 3);
		} else {
			/*
			 * make sure to fix the dimension of the matrix
			 */
			assert(M->nrow == M->ncol);
			GMRFLib_ged_add(ged, M->nrow - 1, M->ncol - 1);
		}
		if (sparse) {
			for (k = 0; k < M->elems; k++) {
				i = M->i[k];
				j = M->j[k];
				imin = IMIN(imin, i);
				jmin = IMIN(jmin, j);
			}
		} else {
			for (k = 0; k < M->nrow; k++) {
				i = (int) M->A[k + 0 * M->nrow];
				j = (int) M->A[k + 1 * M->nrow];
				imin = IMIN(imin, i);
				jmin = IMIN(jmin, j);
			}
		}

		if (sparse) {
			ntriples = M->elems;
			for (k = 0; k < M->elems; k++) {
				i = M->i[k];
				j = M->j[k];
				value = M->values[k];
				if (debug) {
					printf("read (i,j,val) = (%d,%d,%g)\n", i, j, value);
				}
				GMRFLib_ged_add(ged, i, j);
			}
		} else {
			ntriples = M->nrow;
			for (k = 0; k < M->nrow; k++) {
				i = (int) M->A[k + 0 * M->nrow];
				j = (int) M->A[k + 1 * M->nrow];
				value = M->A[k + 2 * M->nrow];
				if (debug) {
					printf("read (i,j,val) = (%d,%d,%g)\n", i, j, value);
				}
				GMRFLib_ged_add(ged, i, j);
			}
		}
		/*
		 * I will free matrix M later... 
		 */
	} else {
		/*
		 * read it first to determine if this is a zero-based or one-based graph 
		 */
		GMRFLib_EWRAP0(GMRFLib_io_open(&io, filename, "r"));
		while (1) {
			old_handler = GMRFLib_set_error_handler_off();
			err = GMRFLib_io_read_next(io, &i, "%d");
			GMRFLib_set_error_handler(old_handler);

			if (err == GMRFLib_SUCCESS) {
				/*
				 * then the rest must be present to 
				 */
				GMRFLib_EWRAP0(GMRFLib_io_read_next(io, &j, "%d"));
				GMRFLib_EWRAP0(GMRFLib_io_read_next(io, &value, "%lf"));
				imin = IMIN(imin, i);
				jmin = IMIN(jmin, j);
			} else {
				break;
			}
		}
		GMRFLib_EWRAP0(GMRFLib_io_close(io));

		GMRFLib_ASSERT(((imin == 0 || imin == 1) && (jmin == 0 || jmin == 1)), GMRFLib_ESNH);
		off = (IMIN(imin, jmin) == 1 ? 1 : 0);

		ntriples = 0;
		GMRFLib_EWRAP0(GMRFLib_io_open(&io, filename, "r"));
		while (1) {
			old_handler = GMRFLib_set_error_handler_off();
			err = GMRFLib_io_read_next(io, &i, "%d");
			GMRFLib_set_error_handler(old_handler);

			if (err == GMRFLib_SUCCESS) {
				/*
				 * then the rest must be present to 
				 */
				GMRFLib_EWRAP0(GMRFLib_io_read_next(io, &j, "%d"));
				GMRFLib_EWRAP0(GMRFLib_io_read_next(io, &value, "%lf"));
				GMRFLib_ged_add(ged, i - off, j - off);

				if (debug)
					printf("read (i,j,val) = (%d,%d,%g)\n", i, j, value);
				ntriples++;
			} else {
				break;
			}
		}
		GMRFLib_EWRAP0(GMRFLib_io_close(io));
	}

	/*
	 * make sure to add all nodes inbetween 
	 */
	for (i = 0; i < GMRFLib_ged_max_node(ged); i++) {
		GMRFLib_ged_add(ged, i, i);
	}
	GMRFLib_ged_build(graph, ged);
	GMRFLib_ged_free(ged);

	/*
	 * step 2. build the tabulate_Qfunc structure 
	 */
	*tabulate_Qfunc = Calloc(1, GMRFLib_tabulate_Qfunc_tp);
	(*tabulate_Qfunc)->Qfunc = GMRFLib_tabulate_Qfunction; /* the Qfunction to use */
	arg = Calloc(1, GMRFLib_tabulate_Qfunc_arg_tp);
	(*tabulate_Qfunc)->Qfunc_arg = (void *) arg;

	arg->n = (*graph)->n;
	arg->values = Calloc((*graph)->n, map_id *);
	arg->prec = prec;
	arg->log_prec = (prec == NULL ? log_prec : NULL);
	if (prec == NULL && log_prec == NULL && log_prec_omp != NULL) {
		int tmax = GMRFLib_MAX_THREADS;
		arg->log_prec_omp = Calloc(tmax, double *);
		for (i = 0; i < tmax; i++) {
			arg->log_prec_omp[i] = log_prec_omp[i];
		}
	} else {
		arg->log_prec_omp = NULL;
	}

	/*
	 * allocate hash-table with the *correct* number of elements
	 */
	for (i = 0; i < (*graph)->n; i++) {
		arg->values[i] = Calloc(1, map_id);
		map_id_init_hint(arg->values[i], (*graph)->lnnbs[i] + 1);
	}

	/*
	 * fill all entries in the graph with zero 
	 */
	for (i = 0; i < (*graph)->n; i++) {
		map_id_set(arg->values[i], i, 0.0);
		for (jj = 0; jj < (*graph)->lnnbs[i]; jj++) {
			j = (*graph)->lnbs[i][jj];
			map_id_set(arg->values[i], j, 0.0);
		}
	}

	/*
	 * then read the file again 
	 */
	if (M) {
		/*
		 * ...or we have it already 
		 */
		if (sparse) {
			for (k = 0; k < M->elems; k++) {
				i = M->i[k];
				j = M->j[k];
				value = M->values[k];
				if (i <= j) {
					i = i - off;
					j = j - off;
					ii = IMIN(i, j);
					jj = IMAX(i, j);
					CHECK_FOR_MULTIPLE_ENTRIES(arg->values[ii], jj, value);
					map_id_set(arg->values[ii], jj, value + PREVIOUS_VALUE);
					if (debug) {
						printf("set (i,j,val) = (%d,%d,%g)\n", i, j, value + PREVIOUS_VALUE);
					}
				}
			}
		} else {
			for (k = 0; k < M->nrow; k++) {
				i = (int) M->A[k + 0 * M->nrow];
				j = (int) M->A[k + 1 * M->nrow];
				value = M->A[k + 2 * M->nrow];

				if (i <= j) {
					i = i - off;
					j = j - off;
					ii = IMIN(i, j);
					jj = IMAX(i, j);
					CHECK_FOR_MULTIPLE_ENTRIES(arg->values[ii], jj, value);
					map_id_set(arg->values[ii], jj, value + PREVIOUS_VALUE);
					if (debug) {
						printf("set (i,j,val) = (%d,%d,%g)\n", i, j, value + PREVIOUS_VALUE);
					}
				}
			}
		}
	} else {
		GMRFLib_EWRAP0(GMRFLib_io_open(&io, filename, "r"));
		k = 0;
		while (k < ntriples) {
			GMRFLib_EWRAP0(GMRFLib_io_read_next(io, &i, "%d"));
			GMRFLib_EWRAP0(GMRFLib_io_read_next(io, &j, "%d"));
			GMRFLib_EWRAP0(GMRFLib_io_read_next(io, &value, "%lf"));

			if (i <= j) {
				i = i - off;
				j = j - off;
				ii = IMIN(i, j);
				jj = IMAX(i, j);
				CHECK_FOR_MULTIPLE_ENTRIES(arg->values[ii], jj, value);
				map_id_set(arg->values[ii], jj, value + PREVIOUS_VALUE);
				if (debug)
					printf("set (i,j,val) = (%d,%d,%g)\n", i, j, value + PREVIOUS_VALUE);
			}
			k++;
		}
		GMRFLib_EWRAP0(GMRFLib_io_close(io));
	}

	GMRFLib_matrix_free(M);
	return GMRFLib_SUCCESS;
}

/*!
  \brief Tabulate a Qfunction defined in a list of (i, j, Qij)

  This function tabulate a Qfunction defined in a list of (i,j,Qij), and setup a Qfunction and its arguments which return these elements. A further argument can
  scale each element, just like a precision. It also computes the corresponding graph. This function is similar to \c GMRFLib_tabulate_Qfunc_from_file() but the
  triplets are now given as arguments in the call to the function instead of defined in a file. Entries not given in the list, have default value zero. If the same
  element (i,j,Qij) is given more than once, then the last element is used.  The indexing can be either zero or one-based. If one-based, then its converted to
  zero-based.

  \param[out] tabulate_Qfunc \a tabulate_Qfunc is a ** of type \a GMRFLib_tabulate_Qfunc_arg_tp, see
  the example.

  \param[out] graph The graph-object computed

  \param[in] ntriples Number of triples of (i,j,Qij)

  \param[in] ilist Vector of i-indices of length \c ntriples

  \param[in] jlist Vector of j-indices of length \c ntriples

  \param[in] Qijlist Vector of Qij-values of length \c ntriples

  \param[in] prec An optional argument which scales the Qfunction (the `precision'). If \c prec is not \c NULL, then
  GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc times *prec. If \c prec is \c NULL, then \c log_prec is tried.

  \param[in] log_prec An optional argument which scales the Qfunction. If \c log_prec is not \c NULL, then
  GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc times * exp(log_prec). If \c log_prec is \c NULL, then \c log_prec_omp is tried.

  \param[in] log_prec_omp An optional argument which precs the Qfunction. If \c log_prec_omp is not \c NULL, then
  GMRFLib_tabulate_Qfunc_tp::Qfunc returns \c Qfunc times * exp(log_prec_omp[GMRFLib_thread_id]).  If \c log_prec_omp is \c
  NULL, then prec is set to 1.0.


*/
int GMRFLib_tabulate_Qfunc_from_list(GMRFLib_tabulate_Qfunc_tp ** tabulate_Qfunc, GMRFLib_graph_tp ** graph,
				     int ntriples, int *ilist, int *jlist, double *Qijlist, int dim, double *prec, double *log_prec,
				     double **log_prec_omp)
{
	/*
	 * as GMRFLib_tabulate_Qfunc(), but get its Q_ij values from its arguments
	 * 
	 * i j Q_{ij} : : : i j Q_{ij}
	 * 
	 */

	int i, imin = INT_MAX, jmin = INT_MAX, off;
	GMRFLib_tabulate_Qfunc_arg_tp *arg = NULL;

	/*
	 * step 1. build the graph 
	 */
	GMRFLib_ged_tp *ged = NULL;

	GMRFLib_ged_init(&ged, NULL);

	for (i = 0; i < ntriples; i++) {
		imin = IMIN(imin, ilist[i]);
		jmin = IMIN(jmin, jlist[i]);
	}
	GMRFLib_ASSERT(((imin == 0 || imin == 1) && (jmin == 0 || jmin == 1)), GMRFLib_ESNH);
	off = (IMIN(imin, jmin) == 1 ? 1 : 0);

	/*
	 * to fix the dimension, possibly padding with zero's 
	 */
	GMRFLib_ged_add(ged, 0, 0);
	if (dim > 0) {
		GMRFLib_ged_add(ged, dim - 1, dim - 1);
	}

	for (i = 0; i < ntriples; i++) {
		GMRFLib_ged_add(ged, ilist[i] - off, jlist[i] - off);
	}

	/*
	 * make sure to add all nodes inbetween 
	 */
	for (i = 0; i < GMRFLib_ged_max_node(ged); i++) {
		GMRFLib_ged_add(ged, i, i);
	}
	GMRFLib_ged_build(graph, ged);
	GMRFLib_graph_prepare(*graph, 0, 0);
	GMRFLib_ged_free(ged);

	/*
	 * step 2. build the tabulate_Qfunc structure 
	 */
	*tabulate_Qfunc = Calloc(1, GMRFLib_tabulate_Qfunc_tp);
	(*tabulate_Qfunc)->Qfunc = GMRFLib_tabulate_Qfunction; /* the Qfunction to use */
	arg = Calloc(1, GMRFLib_tabulate_Qfunc_arg_tp);
	(*tabulate_Qfunc)->Qfunc_arg = (void *) arg;

	arg->n = (*graph)->n;
	arg->values = Calloc((*graph)->n, map_id *);
	if (prec == NULL && log_prec == NULL && log_prec_omp != NULL) {
		int tmax = GMRFLib_MAX_THREADS;
		arg->log_prec_omp = Calloc(tmax, double *);
		for (i = 0; i < tmax; i++) {
			arg->log_prec_omp[i] = log_prec_omp[i];
		}
	} else {
		arg->log_prec_omp = NULL;
	}

	/*
	 * allocate hash-table with the *correct* number of elements
	 */
#pragma omp parallel for private(i)
	for (i = 0; i < (*graph)->n; i++) {
		int j, jj;

		arg->values[i] = Calloc(1, map_id);
		map_id_init_hint(arg->values[i], (*graph)->lnnbs[i] + 1);
		map_id_set(arg->values[i], i, 0.0);
		for (jj = 0; jj < (*graph)->lnnbs[i]; jj++) {
			j = (*graph)->lnbs[i][jj];
			map_id_set(arg->values[i], j, 0.0);    /* fill them with default = 0.0 */
		}
	}

	for (i = 0; i < ntriples; i++) {
		int ii, jj;
		double *prev;

		if (ilist[i] <= jlist[i]) {
			ii = ilist[i] - off;
			jj = jlist[i] - off;
			CHECK_FOR_MULTIPLE_ENTRIES(arg->values[ii], jj, Qijlist[i]);
			map_id_set(arg->values[ii], jj, Qijlist[i] + PREVIOUS_VALUE);
		}
	}

	return GMRFLib_SUCCESS;
}

int GMRFLib_tabulate_Qfunc_from_list2(GMRFLib_tabulate_Qfunc_tp ** tabulate_Qfunc, GMRFLib_graph_tp * graph,
				      int ntriples, int *ilist, int *jlist, double *Qijlist, int UNUSED(dim),
				      double *prec, double *log_prec, double **log_prec_omp)
{
	// this is a special version for Qfunc_rgeneric, as we assume here that graph is know.

	/*
	 * as GMRFLib_tabulate_Qfunc(), but get its Q_ij values from its arguments
	 * 
	 * i j Q_{ij} : : : i j Q_{ij}
	 * 
	 */

	int i, imin = INT_MAX, jmin = INT_MAX, off;
	GMRFLib_tabulate_Qfunc_arg_tp *arg = NULL;

	for (i = 0; i < ntriples; i++) {
		imin = IMIN(imin, ilist[i]);
		jmin = IMIN(jmin, jlist[i]);
	}
	GMRFLib_ASSERT(((imin == 0 || imin == 1) && (jmin == 0 || jmin == 1)), GMRFLib_ESNH);
	off = (IMIN(imin, jmin) == 1 ? 1 : 0);

	*tabulate_Qfunc = Calloc(1, GMRFLib_tabulate_Qfunc_tp);
	(*tabulate_Qfunc)->Qfunc = GMRFLib_tabulate_Qfunction; /* the Qfunction to use */
	arg = Calloc(1, GMRFLib_tabulate_Qfunc_arg_tp);
	(*tabulate_Qfunc)->Qfunc_arg = (void *) arg;

	arg->n = graph->n;
	arg->values = Calloc(graph->n, map_id *);
	if (prec == NULL && log_prec == NULL && log_prec_omp != NULL) {
		int tmax = GMRFLib_MAX_THREADS;
		arg->log_prec_omp = Calloc(tmax, double *);
		for (i = 0; i < tmax; i++) {
			arg->log_prec_omp[i] = log_prec_omp[i];
		}
	} else {
		arg->log_prec_omp = NULL;
	}

	/*
	 * allocate hash-table with the *correct* number of elements
	 */
//#pragma omp parallel for private(i)
	for (i = 0; i < graph->n; i++) {
		int j, jj;

		arg->values[i] = Calloc(1, map_id);
		map_id_init_hint(arg->values[i], graph->lnnbs[i] + 1);
		map_id_set(arg->values[i], i, 0.0);
		for (jj = 0; jj < graph->lnnbs[i]; jj++) {
			j = graph->lnbs[i][jj];
			map_id_set(arg->values[i], j, 0.0);    /* fill them with default = 0.0 */
		}
	}

	for (i = 0; i < ntriples; i++) {
		int ii, jj;
		double *prev;

		if (ilist[i] <= jlist[i]) {
			ii = ilist[i] - off;
			jj = jlist[i] - off;
			CHECK_FOR_MULTIPLE_ENTRIES(arg->values[ii], jj, Qijlist[i]);
			map_id_set(arg->values[ii], jj, Qijlist[i] + PREVIOUS_VALUE);
		}
	}

	return GMRFLib_SUCCESS;
}


/*!
  \brief Free a tabulate_Qfunc-object

  \param[in] tabulate_Qfunc The object of type \c GMRFLib_tabulate_Qfunc_tp to be freed.
*/
int GMRFLib_free_tabulate_Qfunc(GMRFLib_tabulate_Qfunc_tp * tabulate_Qfunc)
{
	if (!tabulate_Qfunc)
		return GMRFLib_SUCCESS;

	int i;
	GMRFLib_tabulate_Qfunc_arg_tp *arg = NULL;

	if (tabulate_Qfunc) {
		arg = (GMRFLib_tabulate_Qfunc_arg_tp *) tabulate_Qfunc->Qfunc_arg;
		if (arg->graph) {
			GMRFLib_graph_free(arg->graph);
		}
		if (arg->Q) {
			GMRFLib_csr_free(&(arg->Q));
		}
		if (arg->Q_idx) {
			for (i = 0; i < arg->n; i++) {
				map_ii_free(arg->Q_idx[i]);
				Free(arg->Q_idx[i]);
			}
			Free(arg->Q_idx);
		}
		if (arg->values) {
			for (i = 0; i < arg->n; i++) {
				map_id_free(arg->values[i]);
				Free(arg->values[i]);
			}
			Free(arg->values);
		}

		Free(arg->log_prec_omp);
		Free(arg);
		Free(tabulate_Qfunc);
	}

	return GMRFLib_SUCCESS;
}
#undef CHECK_FOR_MULTIPLE_ENTRIES
#undef PREVIOUS_VALUE
