#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <complex.h>
#include <math.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <termios.h>
#include <unistd.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_interp.h>
#include <gsl/gsl_interp2d.h>
#include <gsl/gsl_integration.h>

#include "utils.h"

int ispwr2(int N, int *k)
// returns 1 if N = 2^k, 0 otherwise
// k is written into return value
{//{{{
    *k = 0;
    int temp = 1;
    while (temp < N)
    {
        temp <<= 1;
        *k += 1;
    }
    return ((temp == N) ? 1 : 0);
}//}}}

void linspace(int N, double xmin, double xmax, double *x)
{//{{{
    for (int ii=0; ii<N; ii++)
    {
        x[ii] = xmin + (double)(ii)*(xmax-xmin)/(double)(N-1);
    }
}//}}}

void logspace(int N, double xmin, double xmax, double *x)
{//{{{
    for (int ii=0; ii<N; ii++)
    {
        x[ii] = exp(log(xmin) + (double)(ii)*log(xmax/xmin)/(double)(N-1));
    }
}//}}}

void reverse(int N, double *in, double *out)
{//{{{
    if (in == out)
    {
        for (int ii=0; ii<N/2; ii++)
        {
            double temp = in[ii];
            in[ii] = in[N-1-ii];
            in[N-1-ii] = temp;
        }
    }
    else
    {
        for (int ii=0; ii<N; ii++)
        {
            out[ii] = in[N-1-ii];
        }
    }
}//}}}

void zero_real(int N, double *x)
{//{{{
    for (int ii=0; ii<N; ii++)
    {
        x[ii] = 0.0;
    }
}//}}}

void zero_comp(int N, complex *x)
{//{{{
    for (int ii=0; ii<N; ii++)
    {
        x[ii] = 0.0;
    }
}//}}}

int wait(void)
{//{{{
    int ch;
    struct termios oldt, newt;
    tcgetattr ( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr ( STDIN_FILENO, TCSANOW, &newt );
    ch = getchar();
    tcsetattr ( STDIN_FILENO, TCSANOW, &oldt );
    return ch;
}//}}}

struct gnuplot_s
{//{{{
    FILE *gp;
    int Nlines;
};//}}}

gnuplot *plot(gnuplot *gp, int N, double *x, double *y)
{//{{{
    if (gp == NULL)
    {
        gp = (gnuplot *)malloc(sizeof(gnuplot));
        gp->gp = popen("gnuplot", "w");
        gp->Nlines = 0;
        fprintf(gp->gp, "plot '-' with lines, '' with lines , '' with lines , '' with lines, '' with lines\n");
        // holds maximum of 5 lines
    }
    for (int ii=0; ii<N; ii++)
    {
        fprintf(gp->gp, "%.8e %.8e\n", x[ii], y[ii]);
    }
    fprintf(gp->gp, "e\n");
    gp->Nlines += 1;
    return gp;
}//}}}

gnuplot *plot_comp(gnuplot *gp, int N, double *x, complex *y, int mode)
{//{{{
    if (gp == NULL)
    {
        gp = (gnuplot *)malloc(sizeof(gnuplot));
        gp->gp = popen("gnuplot", "w");
        gp->Nlines = 0;
        fprintf(gp->gp, "plot '-' with lines, '' with lines , '' with lines , '' with lines, '' with lines\n");
        // holds maximum of 5 lines
    }
    for (int ii=0; ii<N; ii++)
    {
        fprintf(gp->gp, "%.8e %.8e\n", x[ii], (mode==0) ? creal(y[ii]) : cimag(y[ii]));
    }
    fprintf(gp->gp, "e\n");
    gp->Nlines += 1;
    return gp;
}//}}}

void show(gnuplot *gp)
{//{{{
    for (int ii=0; ii<(5 - gp->Nlines); ii++)
    {
        fprintf(gp->gp, "e\n");
    }
    fflush(gp->gp);
    wait();
    fclose(gp->gp);
    free(gp);
}//}}}

static
int lcounttxt(FILE *f)
{//{{{
    //printf("----------COUNTING LINES IN--------------\n");
    fseek(f, 0, SEEK_SET);
    int ctr = 0;
    while (1)
    {
        //if (ctr < 20)
        //{
        //    printf("%c", ch);
        //}
        int ch = fgetc(f);
        if (feof(f))
        {
            break;
        }
        if (ch == 10 /*new line character*/)
        {
            ++ctr;
        }
    }
    fseek(f, 0, SEEK_SET);
    return ctr;
}//}}}

static
int lcountb(FILE *f, int Nvec)
{//{{{
    fseek(f, 0, SEEK_END);
    int len = ftell(f) / sizeof(double);
    fseek(f, 0, SEEK_SET);
    if (len%Nvec)
    {
        printf("File not possible.\n");
        return 0;
    }
    return len / Nvec;
}//}}}

void savetxt(char *fname, int Nlines, int Nvec, ...)
{//{{{
    va_list valist;
    va_start(valist, Nvec);
    double **x = (double **)malloc(Nvec * sizeof(double *));
    for (int ii=0; ii<Nvec; ii++)
    {
        x[ii] = va_arg(valist, double *);
    }
    va_end(valist);

    char unit[256];
    sprintf(unit, "%+25.16e ", -M_PI);
    int len = strlen(unit);
    char line[512];

    FILE *f = fopen(fname, "w");
    for (int ii=0; ii<Nlines; ii++)
    {
        for (int jj=0; jj<Nvec; jj++)
        {
            sprintf(line+jj*len, "%+25.16e ", x[jj][ii]);
        }
        fprintf(f, "%s\n", line);
    }
    free(x);
    fclose(f);
}//}}}

double **loadtxt(char *fname, int *Nlines, int Nvec)
{//{{{
    FILE *f = fopen(fname, "r");
    if (f == NULL)
    {
        printf("File %s not found.\n", fname);
        return NULL;
    }
    *Nlines = lcounttxt(f);

    // allocate the vectors to the correct length
    double **x = (double **)malloc(Nvec * sizeof(double *));
    for (int ii=0; ii<Nvec; ii++)
    {
        x[ii] = (double *)malloc(*Nlines * sizeof(double));
    }
    // prepare the formatters
    char format[Nvec][512];
    char unit_read[] = "%lf";
    char unit_ignore[] = "%*lf";
    for (int ii=0; ii<Nvec; ii++)
    {
        int space_written = 0;
        for (int jj=0; jj<Nvec; jj++)
        {
            if (jj==ii)
            {
                sprintf(format[ii]+space_written, "%s", unit_read);
                space_written += strlen(unit_read);
            }
            else
            {
                sprintf(format[ii]+space_written, "%s", unit_ignore);
                space_written += strlen(unit_ignore);
            }
        }
    }
    // now read the file
    char line[4096];
    for (int ii=0; ii<*Nlines; ii++)
    {
        if(fgets(line, 4096, f) == NULL)
        {
            printf("Line number not correct.\n");
            break;
        }
        for (int jj=0; jj<Nvec; jj++)
        {
            sscanf(line, format[jj], x[jj]+ii);
        }
    }
    fclose(f);

    return x;
}//}}}

void tofile(char *fname, int Nlines, int Nvec, ...)
{//{{{
    va_list valist;
    va_start(valist, Nvec);
    FILE *f = fopen(fname, "wb");

    for (int ii=0; ii<Nvec; ii++)
    {
        double *x = va_arg(valist, double *);
        fwrite(x, sizeof(double), Nlines, f);
    }
    fclose(f);
    va_end(valist);
}//}}}

double **fromfile(char *fname, int *Nlines, int Nvec)
{//{{{
    FILE *f = fopen(fname, "rb");
    if (f == NULL)
    {
        printf("File %s not found.\n", fname);
        return NULL;
    }
    *Nlines = lcountb(f, Nvec);

    // allocate the vectors to the correct length
    double **x = (double **)malloc(Nvec * sizeof(double *));
    for (int ii=0; ii<Nvec; ii++)
    {
        x[ii] = (double *)malloc(*Nlines * sizeof(double));
    }

    // read into the output arrays
    for (int ii=0; ii<Nvec; ii++)
    {
        int read = fread(x[ii], sizeof(double), *Nlines, f);
        if (read != *Nlines)
        {
            printf("File %s corrupted.\n", fname);
            return NULL;
        }
    }

    return x;
}//}}}

int isfile(char *fname)
{//{{{
    FILE *f = fopen(fname, "r");
    if (f == NULL)
    {
        return 0;
    }
    else
    {
        fclose(f);
        return 1;
    }
}//}}}

int this_core(void)
// TODO
{//{{{
    #ifdef _OPENMP
    return omp_get_thread_num();
    #else
    return 0;
    #endif
}//}}}

struct interp1d_s
{//{{{
    gsl_interp *i;
    gsl_interp_accel *a;
    int alloced_accel;
    int N;
    double *x;
    double *y;
    // values requested below xmin/above xmax are replaced by ylo/yhi
    double ylo;
    double yhi;
};//}}}

interp1d *new_interp1d(int N, double *x, double *y, double ylo, double yhi,
                       interp_mode m, gsl_interp_accel *a)
{//{{{
    const gsl_interp_type *T;
    switch (m)
    {
        case interp_linear           : T = gsl_interp_linear; break;
        case interp_polynomial       : T = gsl_interp_polynomial; break;
        case interp_cspline          : T = gsl_interp_cspline; break;
        case interp_cspline_periodic : T = gsl_interp_cspline_periodic; break;
        case interp_akima            : T = gsl_interp_akima; break;
        case interp_akima_periodic   : T = gsl_interp_akima_periodic; break;
        case interp_steffen          : T = gsl_interp_steffen; break;
        default                      : printf("Unknown gsl_interp_type.\n");
                                       return NULL;
    }
    interp1d *out = malloc(sizeof(interp1d));
    out->N = N;
    out->x = x;
    out->y = y;
    out->ylo = ylo;
    out->yhi = yhi;
    out->i = gsl_interp_alloc(T, N);
    if (a == NULL)
    {
        out->alloced_accel = 1;
        out->a = gsl_interp_accel_alloc();
    }
    else
    {
        out->alloced_accel = 0;
        out->a = a;
    }
    gsl_interp_init(out->i, out->x, out->y, out->N);
    return out;
}//}}}

void delete_interp1d(interp1d *interp)
{//{{{
    gsl_interp_free(interp->i);
    if (interp->alloced_accel)
    {
        gsl_interp_accel_free(interp->a);
    }
}//}}}

double interp1d_eval(interp1d *interp, double x)
{//{{{
    if (x < interp->x[0])
    {
        return interp->ylo;
    }
    else if (x > interp->x[interp->N-1])
    {
        return interp->yhi;
    }
    else
    {
        return gsl_interp_eval(interp->i, interp->x, interp->y, x, interp->a);
    }
}//}}}

double interp1d_eval_deriv(interp1d *interp, double x)
{//{{{
    if (x < interp->x[0])
    {
        return 0.0;
    }
    else if (x > interp->x[interp->N-1])
    {
        return 0.0;
    }
    else
    {
        return gsl_interp_eval_deriv(interp->i, interp->x, interp->y, x, interp->a);
    }
}//}}}

double interp1d_eval_integ(interp1d *interp, double a, double b)
{//{{{
    double _a = GSL_MAX(a, interp->x[0]); // _a >= a
    double _b = GSL_MIN(b, interp->x[interp->N-1]); // _b <= b
    if (_a >= _b)
    {
        return 0;
    }
    else
    {
        return gsl_interp_eval_integ(interp->i, interp->x, interp->y, _a, _b, interp->a)
               + (_a - a) * interp->ylo + (b - _b) * interp->yhi;
    }
}//}}}

struct interp2d_s
// on symmetric 2d grid
{//{{{
    gsl_interp2d *i;
    gsl_interp_accel *a;
    int alloced_accel;
    int N;
    double *x;
    double *z;
    double zlo;
    double zhi;
};//}}}

interp2d *new_interp2d(int N, double *x, double *z,
                       double zlo, double zhi,
                       interp2d_mode m, gsl_interp_accel *a)
{//{{{
    const gsl_interp2d_type *T;
    switch (m)
    {
        case interp2d_bilinear : T = gsl_interp2d_bilinear; break;
        case interp2d_bicubic  : T = gsl_interp2d_bicubic; break;
        default                : printf("Unknown gsl_interp2d_type.\n");
                                 return NULL;
    }
    interp2d *out = malloc(sizeof(interp2d));
    out->N = N;
    out->x = x;
    out->z = z;
    out->zlo = zlo;
    out->zhi = zhi;
    out->i = gsl_interp2d_alloc(T, out->N, out->N);
    if (a == NULL)
    {
        out->alloced_accel = 1;
        out->a = gsl_interp_accel_alloc();
    }
    else
    {
        out->alloced_accel = 0;
        out->a = a;
    }
    gsl_interp2d_init(out->i, out->x, out->x, out->z, out->N, out->N);
    return out;
}//}}}

void delete_interp2d(interp2d *interp)
{//{{{
    gsl_interp2d_free(interp->i);
    if (interp->alloced_accel)
    {
        gsl_interp_accel_free(interp->a);
    }
}//}}}

double interp2d_eval(interp2d *interp, double x, double y)
{//{{{
    if (x < interp->x[0] || y < interp->x[0])
    {
        return interp->zlo;
    }
    else if (x > interp->x[interp->N-1] || y > interp->x[interp->N-1])
    {
        return interp->zhi;
    }
    else
    {
        return gsl_interp2d_eval(interp->i, interp->x, interp->x, interp->z,
                                 x, y, interp->a, interp->a);
    }
}//}}}

// CAUTION : these binning functions assume that x is equally spaced,
//           and that y,z are normalized to unit sum.
//           They normalize the output accordingly.

void bin_1d(int N, double *x, double *y,
            int Nbins, double *binedges, double *out, interp_mode m)
{//{{{
    interp1d *interp = new_interp1d(N, x, y, 0.0, 0.0, m, NULL);
    for (int ii=0; ii<Nbins; ii++)
    {
        // TODO normalization
        out[ii] = interp1d_eval_integ(interp, binedges[ii], binedges[ii+1])
                  / (x[1] - x[0]);
    }
    delete_interp1d(interp);
}//}}}

void bin_2d(int N, double *x, double *z, int Nsample,
            int Nbins, double *binedges, double *out, interp2d_mode m)
{//{{{
    interp2d *interp = new_interp2d(N, x, z, 0.0, 0.0, m, NULL);
    gsl_integration_glfixed_table *t = gsl_integration_glfixed_table_alloc(Nsample);

    for (int ii=0; ii<Nbins; ii++)
    {
        for (int jj=0; jj<Nbins; jj++)
        {
            double *res = out + ii*Nbins + jj;
            *res = 0.0;

            for (int kk=0; kk<Nsample; kk++)
            {
                double node_i, weight_i;
                gsl_integration_glfixed_point(binedges[ii], binedges[ii+1],
                                              kk, &node_i, &weight_i, t);

                for (int ll=0; ll<Nsample; ll++)
                {
                    double node_j, weight_j;
                    gsl_integration_glfixed_point(binedges[jj], binedges[jj+1],
                                                  ll, &node_j, &weight_j, t);

                    *res += weight_i * weight_j * interp2d_eval(interp, node_i, node_j)
                            / gsl_pow_2(x[1] - x[0]);
                }
            }
        }
    }

    gsl_integration_glfixed_table_free(t);
    delete_interp2d(interp);
}//}}}
