#ifndef ONEPOINT_H
#define ONEPOINT_H

#include "hmpdf.h"

typedef struct
{//{{{
    int created_op;
    double *PDFu;
    double *PDFc;

    int created_noisy_op;
    double *PDFu_noisy;
    double *PDFc_noisy;

    double signalmeanu;
    double signalmeanc;
}//}}}
onepoint_t;

int null_onepoint(hmpdf_obj *d);
int reset_onepoint(hmpdf_obj *d);
int create_op(hmpdf_obj *d);
int hmpdf_get_op(hmpdf_obj *d, int Nbins, double *binedges, double *out, int incl_2h, int noisy);

#endif
