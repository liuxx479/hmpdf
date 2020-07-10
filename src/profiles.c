#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include <gsl/gsl_math.h>
#include <gsl/gsl_sf_bessel.h>
#include <gsl/gsl_sf_result.h>
#include <gsl/gsl_interp.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_dht.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_fit.h>

#include "configs.h"
#include "utils.h"
#include "object.h"
#include "cosmology.h"
#include "halo_model.h"
#include "filter.h"
#include "profiles.h"

#include "hmpdf.h"

int
null_profiles(hmpdf_obj *d)
{//{{{
    STARTFCT

    d->p->inited_profiles = 0;
    d->p->decr_tgrid = NULL;
    d->p->incr_tgrid = NULL;
    d->p->decr_tsqgrid = NULL;
    d->p->incr_tsqgrid = NULL;
    d->p->prtilde_thetagrid = NULL;
    d->p->reci_tgrid = NULL;
    d->p->created_monotonicity = 0;
    d->p->is_not_monotonic = NULL;
    d->p->created_segments = 0;
    d->p->segment_boundaries = NULL;
    d->p->dht_ws = NULL;
    d->p->profiles = NULL;
    d->p->created_conj_profiles = 0;
    d->p->conj_profiles = NULL;
    d->p->created_filtered_profiles = 0;
    d->p->incr_tgrid_accel = NULL;
    d->p->reci_tgrid_accel = NULL;

    ENDFCT
}//}}}

int
reset_profiles(hmpdf_obj *d)
{//{{{
    STARTFCT
    
    HMPDFPRINT(2, "\treset_profiles\n")

    if (d->p->decr_tgrid != NULL) { free(d->p->decr_tgrid); }
    if (d->p->incr_tgrid != NULL) { free(d->p->incr_tgrid); }
    if (d->p->decr_tsqgrid != NULL) { free(d->p->decr_tsqgrid); }
    if (d->p->incr_tsqgrid != NULL) { free(d->p->incr_tsqgrid); }
    if (d->p->reci_tgrid != NULL) { free(d->p->reci_tgrid); }
    if (d->p->prtilde_thetagrid != NULL) { free(d->p->prtilde_thetagrid); }
    if (d->p->incr_tgrid_accel != NULL)
    {
        for (int ii=0; ii<d->Ncores; ii++)
        {
            if (d->p->incr_tgrid_accel[ii] != NULL)
            {
                gsl_interp_accel_free(d->p->incr_tgrid_accel[ii]);
            }
        }
        free(d->p->incr_tgrid_accel);
    }
    if (d->p->reci_tgrid_accel != NULL) { gsl_interp_accel_free(d->p->reci_tgrid_accel); }
    if (d->p->profiles != NULL)
    {
        for (int z_index=0; z_index<d->n->Nz; z_index++)
        {
            if (d->p->profiles[z_index] != NULL)
            {
                for (int M_index=0; M_index<d->n->NM; M_index++)
                {
                    if (d->p->profiles[z_index][M_index] != NULL)
                    {
                        free(d->p->profiles[z_index][M_index]);
                    }
                }
                free(d->p->profiles[z_index]);
            }
        }
        free(d->p->profiles);
    }
    if (d->p->conj_profiles != NULL)
    {
        for (int z_index=0; z_index<d->n->Nz; z_index++)
        {
            if (d->p->conj_profiles[z_index] != NULL)
            {
                for (int M_index=0; M_index<d->n->NM; M_index++)
                {
                    if (d->p->conj_profiles[z_index][M_index] != NULL)
                    {
                        free(d->p->conj_profiles[z_index][M_index]);
                    }
                }
                free(d->p->conj_profiles[z_index]);
            }
        }
        free(d->p->conj_profiles);
    }
    if (d->p->is_not_monotonic != NULL)
    {
        for (int z_index=0; z_index<d->n->Nz; z_index++)
        {
            if (d->p->is_not_monotonic[z_index] != NULL)
            {
                free(d->p->is_not_monotonic[z_index]);
            }
        }
        free(d->p->is_not_monotonic);
    }
    if (d->p->segment_boundaries != NULL)
    {
        for (int z_index=0; z_index<d->n->Nz; z_index++)
        {
            if (d->p->segment_boundaries[z_index] != NULL)
            {
                for (int M_index=0; M_index<d->n->NM; M_index++)
                {
                    if (d->p->segment_boundaries[z_index][M_index] != NULL)
                    {
                        free(d->p->segment_boundaries[z_index][M_index]);
                    }
                }
                free(d->p->segment_boundaries[z_index]);
            }
        }
        free(d->p->segment_boundaries);
    }
    if (d->p->dht_ws != NULL) { gsl_dht_free(d->p->dht_ws); }

    ENDFCT
}//}}}

static int
create_angle_grids(hmpdf_obj *d)
{//{{{
    STARTFCT

    HMPDFPRINT(2, "\tcreate_angle_grids\n")
    
    d->p->prtilde_Ntheta = PRTILDE_INTEGR_NTHETA;

    SAFEALLOC(, d->p->decr_tgrid,   malloc((d->p->Ntheta+1) * sizeof(double)))
    SAFEALLOC(, d->p->incr_tgrid,   malloc((d->p->Ntheta+1) * sizeof(double)))
    SAFEALLOC(, d->p->decr_tsqgrid, malloc((d->p->Ntheta+1) * sizeof(double)))
    SAFEALLOC(, d->p->incr_tsqgrid, malloc((d->p->Ntheta+1) * sizeof(double)))
    SAFEALLOC(, d->p->reci_tgrid,   malloc(d->p->Ntheta     * sizeof(double)))
    // TODO rename, perhaps we don't need it!
    SAFEALLOC(, d->p->prtilde_thetagrid, malloc(d->p->prtilde_Ntheta * sizeof(double)))

    for (int ii=0; ii<d->p->Ntheta; ii++)
    {
        // reverse order, maximum angle is the first one
        gsl_sf_result result1, result2;
        SAFEGSL(gsl_sf_bessel_zero_J0_e(ii+1, &result1))
        SAFEGSL(gsl_sf_bessel_zero_J0_e(d->p->Ntheta, &result2))
        d->p->decr_tgrid[d->p->Ntheta-1-ii] = result1.val / result2.val;
        d->p->decr_tsqgrid[d->p->Ntheta-1-ii]
            = gsl_pow_2(d->p->decr_tgrid[d->p->Ntheta-1-ii]);

        d->p->reci_tgrid[ii] = result1.val;
    }

    reverse(d->p->Ntheta, d->p->decr_tgrid, d->p->incr_tgrid+1);
    reverse(d->p->Ntheta, d->p->decr_tsqgrid, d->p->incr_tsqgrid+1);
    // fix the endpoints
    d->p->decr_tgrid[d->p->Ntheta] = 0.0;
    d->p->decr_tsqgrid[d->p->Ntheta] = 0.0;
    d->p->incr_tsqgrid[0] = 0.0;
    d->p->incr_tgrid[0] = 0.0;

    linspace(d->p->prtilde_Ntheta, 0.0, 1.0, d->p->prtilde_thetagrid);
    SAFEALLOC(, d->p->incr_tgrid_accel,
              malloc(d->Ncores * sizeof(gsl_interp_accel *)))
    for (int ii=0; ii<d->Ncores; ii++)
    {
        SAFEALLOC(, d->p->incr_tgrid_accel[ii], gsl_interp_accel_alloc())
    }
    SAFEALLOC(, d->p->reci_tgrid_accel, gsl_interp_accel_alloc())

    ENDFCT
}//}}}

static int
fix_endpoints(int N, double *x, double *y)
// sets y[0]=0 and y[N] to linear extrapolation
{//{{{
    STARTFCT

    y[0] = 0.0;
    double a = (y[N-1]-y[N-2]) / (x[N-1]-x[N-2]);
    double b = y[N-1] - a * x[N-1];
    y[N] = a * x[N] + b;

    ENDFCT
}//}}}

static int
kappa_profile(hmpdf_obj *d, int z_index, int M_index,
              double theta_out, double Rout, double *p)
{//{{{
    STARTFCT

    // find the NFW parameters
    double rhos, rs;
    SAFEHMPDF(NFW_fundamental(d, z_index, M_index, &rhos, &rs))

    // fill the profile
    for (int ii=0; ii<d->p->Ntheta; ii++)
    {
        double t = d->p->decr_tgrid[ii] * theta_out;
        double Rproj = tan(t) * d->c->angular_diameter[z_index];
        double lout = sqrt(Rout*Rout - Rproj*Rproj);

        if (Rproj > rs)
        {
            p[ii] = 2.0*rhos*gsl_pow_3(rs)/(2.0*(Rout+rs)*pow((Rproj-rs)*(Rproj+rs),1.5))
                            *(+M_PI*rs*(Rout+rs)
                              +2.0*sqrt((Rout-Rproj)*(Rout+Rproj)*(Rproj-rs)*(Rproj+rs))
                              -2.0*rs*(Rout+rs)*atan2(Rproj*Rproj+Rout*rs,                
                                                      -sqrt((Rout-Rproj)*(Rout+Rproj)          
                                                            *(Rproj-rs)*(Rproj+rs))));
        }
        else if (Rproj < rs)
        {
            p[ii] = 2.0*rhos*gsl_pow_3(rs)/((Rout+rs)*pow(rs*rs-Rproj*Rproj,1.5))
                            *(-sqrt((Rout-Rproj)*(Rout+Rproj)*(rs*rs-Rproj*Rproj))
                              +rs*(Rout+rs)*log(Rproj*(Rout+rs)/(Rproj*Rproj+Rout*rs
                                                                 -sqrt((Rout-Rproj)*(Rout+Rproj)
                                                                       *(rs*rs-Rproj*Rproj)))));
        }
        else
        {
            p[ii] = 2.0*rhos*sqrt(Rout-rs)*rs*(Rout+2.0*rs)/(3.0*pow(Rout+rs,1.5));
        }
        // TODO check if rho_m here physical or comoving
        p[ii] -= 2.0*lout*d->c->rho_m[z_index];
        p[ii] /= d->c->Scrit[z_index];
    }

    ENDFCT
}//}}}

// Battaglia profiles{{{
typedef struct
{
    double alpha;
    double beta;
    double gamma;
    double rproj;
}
Battmodel_params;

static double
_Battmodel_primitive(hmpdf_obj *d, double M200c, double z, int n)
{
    return d->p->Battaglia12_params[n*3+0]
           * pow(M200c/1e14, d->p->Battaglia12_params[n*3+1])
           * pow(1.0+z, d->p->Battaglia12_params[n*3+2]);
}
static double
Battmodel_integrand(double z, void *params)
{
    Battmodel_params *p = (Battmodel_params *)params;
    double r = hypot(z, p->rproj);
    return pow(r, p->gamma) / pow(1.0 + pow(r, p->alpha), p->beta);
}
static int
tsz_profile(hmpdf_obj *d, int z_index, int M_index,
            double theta_out, double Rout, double *p)
{
    STARTFCT

    // convert to 200c
    double M200c, R200c, c200c;
    SAFEHMPDF(Mconv(d, z_index, M_index, hmpdf_mdef_c, &M200c, &R200c, &c200c))
    double P0 = _Battmodel_primitive(d, M200c, d->n->zgrid[z_index], 0);
    double xc = _Battmodel_primitive(d, M200c, d->n->zgrid[z_index], 1);
    Rout /= R200c * xc;

    // prepare the integration
    Battmodel_params par;
    par.alpha = _Battmodel_primitive(d, M200c, d->n->zgrid[z_index], 2);
    par.beta  = _Battmodel_primitive(d, M200c, d->n->zgrid[z_index], 3);
    par.gamma = _Battmodel_primitive(d, M200c, d->n->zgrid[z_index], 4);

    gsl_function integrand;
    integrand.function = &Battmodel_integrand;
    integrand.params = &par;
    SAFEALLOC(gsl_integration_workspace *, ws,
              gsl_integration_workspace_alloc(BATTINTEGR_LIMIT))

    // loop over angles
    for (int ii=1/*start one inside, outermost value=0*/; ii<d->p->Ntheta; ii++)
    {
        double t = d->p->decr_tgrid[ii] * theta_out;
        par.rproj = tan(t) * d->c->angular_diameter[z_index] / R200c / xc;
        double lout = sqrt(Rout*Rout - par.rproj*par.rproj);
        
        double err;
        SAFEGSL(gsl_integration_qag(&integrand, 0.0, lout,
                                    BATTINTEGR_EPSABS, BATTINTEGR_EPSREL,
                                    BATTINTEGR_LIMIT, BATTINTEGR_KEY,
                                    ws, p+ii, &err))

        // normalize
        p[ii] *= P0 * xc * M200c * 200.0
                 * d->c->rho_c[z_index] * d->c->Ob_0 / d->c->Om_0
                 * GNEWTON * SIGMATHOMSON / MELECTRON / gsl_pow_2(SPEEDOFLIGHT)
                 / 1.932; // convert from thermal to electron pressure
    }

    gsl_integration_workspace_free(ws);

    ENDFCT
}
//}}}

static int
profile(hmpdf_obj *d, int z_index, int M_index, double *p)
// returns theta_out and writes the profile into return value
{//{{{
    STARTFCT

    // find the outer radius on the sky
    double M, Rout, c;
    SAFEHMPDF(Mconv(d, z_index, M_index, d->p->rout_def, &M, &Rout, &c))
    Rout *= d->p->rout_scale;
    double theta_out = atan(Rout/d->c->angular_diameter[z_index]);

    if (d->p->stype == hmpdf_kappa)
    {
        SAFEHMPDF(kappa_profile(d, z_index, M_index,
                                theta_out, Rout, p+1))
    }
    else if (d->p->stype == hmpdf_tsz)
    {
        SAFEHMPDF(tsz_profile(d, z_index, M_index,
                              theta_out, Rout, p+1))
    }
    else
    {
        HMPDFERR("unkown signal type.")
    }

    p[0] = theta_out;

    ENDFCT
}//}}}

static int
create_profiles(hmpdf_obj *d)
{//{{{
    STARTFCT

    HMPDFPRINT(2, "\tcreate_profiles\n")
    
    SAFEALLOC(, d->p->profiles, malloc(d->n->Nz * sizeof(double **)))
    #ifdef _OPENMP
    #pragma omp parallel for num_threads(d->Ncores)
    #endif
    for (int z_index=0; z_index<d->n->Nz; z_index++)
    {
        if (hmpdf_status) { continue; }
        SAFEALLOC_NORETURN(, d->p->profiles[z_index], malloc(d->n->NM * sizeof(double *)))
        for (int M_index=0; M_index<d->n->NM; M_index++)
        {
            if (hmpdf_status) { continue; }
            SAFEALLOC_NORETURN(, d->p->profiles[z_index][M_index],
                               malloc((d->p->Ntheta+2) * sizeof(double)))
            if (hmpdf_status) { continue; }
            
            SAFEHMPDF_NORETURN(profile(d, z_index, M_index,
                                       d->p->profiles[z_index][M_index]))
            if (hmpdf_status) { continue; }
            
            SAFEHMPDF_NORETURN(fix_endpoints(d->p->Ntheta, d->p->decr_tgrid,
                                             d->p->profiles[z_index][M_index]+1))
            
            if (hmpdf_status) { continue; }
        }
    }

    ENDFCT
}//}}}

int
create_conj_profiles(hmpdf_obj *d)
// computes the conjugate space profiles
{//{{{
    STARTFCT

    if (d->p->created_conj_profiles) { return hmpdf_status; }

    HMPDFPRINT(2, "\tcreate_conj_profiles\n")
    
    // prepare the Hankel transform work space
    d->p->dht_ws = gsl_dht_new(d->p->Ntheta, 0, 1.0);
    SAFEALLOC(, d->p->conj_profiles, malloc(d->n->Nz * sizeof(double **)))
    #ifdef _OPENMP
    #pragma omp parallel for num_threads(d->Ncores)
    #endif
    for (int z_index=0; z_index<d->n->Nz; z_index++)
    {
        if (hmpdf_status) { continue; }
        // need buffer to store the profiles with theta increasing
        // allocate inside z-loop for thread safety
        SAFEALLOC_NORETURN(double *, temp, malloc(d->p->Ntheta * sizeof(double)))
        if (hmpdf_status) { continue; }
        SAFEALLOC_NORETURN(, d->p->conj_profiles[z_index],
                           malloc(d->n->NM * sizeof(double *)))
        if (hmpdf_status) { continue; }
        for (int M_index=0; M_index<d->n->NM; M_index++)
        {
            if (hmpdf_status) { continue; }
            SAFEALLOC_NORETURN(, d->p->conj_profiles[z_index][M_index],
                               malloc((d->p->Ntheta+1) * sizeof(double)))
            reverse(d->p->Ntheta, d->p->profiles[z_index][M_index]+1, temp);
            // dht_ws is const under gsl_dht_apply, so this is thread safe
            SAFEGSL_NORETURN(gsl_dht_apply(d->p->dht_ws, temp,
                                           d->p->conj_profiles[z_index][M_index]+1))
            if (hmpdf_status) { continue; }
            d->p->conj_profiles[z_index][M_index][0]
                = 1.0 / d->p->profiles[z_index][M_index][0];
        }
        if (hmpdf_status) { continue; }
        free(temp);
    }

    d->p->created_conj_profiles = 1;

    ENDFCT
}//}}}

int
create_filtered_profiles(hmpdf_obj *d)
{//{{{
    STARTFCT

    if (d->p->created_filtered_profiles) { return hmpdf_status; }
    if (d->f->Nfilters == 0 ) { return hmpdf_status; }

    HMPDFPRINT(2, "\tcreate_filtered_profiles\n")

    #ifdef _OPENMP
    #pragma omp parallel for num_threads(d->Ncores)
    #endif
    for (int z_index=0; z_index<d->n->Nz; z_index++)
    {
        if (hmpdf_status) { continue; }
        SAFEALLOC_NORETURN(double *, ell, malloc(d->p->Ntheta * sizeof(double)))
        if (hmpdf_status) { continue; }
        SAFEALLOC_NORETURN(double *, temp, malloc(d->p->Ntheta * sizeof(double))) // buffer
        if (hmpdf_status) { continue; }
        for (int M_index=0; M_index<d->n->NM; M_index++)
        {
            if (hmpdf_status) { continue; }
            for (int ii=0; ii<d->p->Ntheta; ii++)
            {
                ell[ii] = d->p->reci_tgrid[ii]
                          * d->p->conj_profiles[z_index][M_index][0];
            }
            // multiply with the window functions
            SAFEHMPDF_NORETURN(apply_filters(d, d->p->Ntheta, ell,
                                             d->p->conj_profiles[z_index][M_index]+1,
                                             temp, 1, filter_pdf, &z_index))
            if (hmpdf_status) { continue; }
            // transform back to real space
            SAFEGSL_NORETURN(gsl_dht_apply(d->p->dht_ws, temp,
                                           d->p->profiles[z_index][M_index]+1))
            if (hmpdf_status) { continue; }
            // reverse the profile
            reverse(d->p->Ntheta, d->p->profiles[z_index][M_index]+1,
                                  d->p->profiles[z_index][M_index]+1);
            // normalize properly
            for (int ii=0; ii<d->p->Ntheta; ii++)
            {
                d->p->profiles[z_index][M_index][ii+1]
                    *= gsl_pow_2(d->p->reci_tgrid[d->p->Ntheta-1]);
            }

            SAFEHMPDF_NORETURN(fix_endpoints(d->p->Ntheta, d->p->decr_tgrid,
                                             d->p->profiles[z_index][M_index]+1))
        }
        if (hmpdf_status) { continue; }
        free(temp);
        free(ell);
    }

    d->p->created_filtered_profiles = 1;

    ENDFCT
}//}}}

static int
monotonize(int Nx, int Nproblems, int *problems, double *x, double *y)
// problems is an int[Nproblems], holding the indices where y was decreasing,
// in increasing order
{//{{{
    STARTFCT

    double scale = 0.0;
    for (int jj=1; jj<Nx; jj++)
    {
        scale += y[jj] * y[jj];
    }
    scale = sqrt(scale/(double)(Nx-1));

    int ledge = 0;
    int redge = 0;
    for (int ii=0; ii<Nproblems; ii++)
    {
        // check if we have already covered this problem
        if (redge > problems[ii])
        {
            continue;
        }
        ledge = problems[ii] - 1; // >= 0
        
        // find the right edge
        redge = Nx;
        for (int jj=ledge+1; jj<Nx; jj++)
        {
            if (y[jj] > y[ledge]+0.1*scale)
            {
                redge = jj;
                break;
            }
        }

        double a, b;
        // treat the case when no redge was found (only decreasing from here onwards)
        // in this case, we build a simple linear ramp, and hope that this profile is not important
        if (redge == Nx)
        {
            if (ledge == 0)
            // case when we have no valid points
            // use stdev as relevant scale
            // profiles using this are completely irrelevant hopefully
            {
                a = scale / (x[Nx-1] - x[ledge]);
                b = - a * x[ledge];
            }
            else
            // perform linear fit through the last few datapoints
            // x       x      x      x    x
            // _start ...   ....   ....  ledge
            {
                int _start = GSL_MAX(0, ledge-3);
                double cov00, cov01, cov11, sumsq;
                SAFEGSL(gsl_fit_linear(x+_start, 1, y+_start, 1, ledge-_start+1,
                                       &b, &a, &cov00, &cov01, &cov11, &sumsq))
                // as is by construction negative, but we still need to ensure
                // b doesn't spoil the party,
                // and ensure numerical stability in a
                a = GSL_MIN(-scale/fabs(x[Nx-1]-x[0]), a);
                b = GSL_MAX(y[ledge] - a * x[ledge], b);
            }
        }
        // a redge was found
        else
        {
            a = (y[redge]-y[ledge]) / (x[redge] - x[ledge]);
            b = y[ledge] - a * x[ledge];
        }

        for (int jj=ledge+1; jj<redge; jj++)
        {
            y[jj] = a * x[jj] + b;
        }
    }
    
    ENDFCT
}//}}}

int
create_monotonicity(hmpdf_obj *d)
{//{{{
    STARTFCT

    if (d->p->created_monotonicity) { return hmpdf_status; }

    HMPDFPRINT(2, "\tcreate_monotonicity\n")

    if (d->n->monotonize)
    {
        HMPDFPRINT(3, "\t\tmonotonizing\n")
    }
    else
    {
        HMPDFPRINT(3, "\t\tnot monotonizing\n")
    }

    SAFEALLOC(, d->p->is_not_monotonic, malloc(d->n->Nz * sizeof(int *)))
    for (int z_index=0; z_index<d->n->Nz; z_index++)
    {
        SAFEALLOC(, d->p->is_not_monotonic[z_index],
                  malloc((d->n->NM+1) * sizeof(int)))
        d->p->is_not_monotonic[z_index][d->n->NM] = 0;
        for (int M_index=0; M_index<d->n->NM; M_index++)
        {
            int _problems[d->p->Ntheta+1];
            d->p->is_not_monotonic[z_index][M_index]
            // FIXME
                = not_monotonic(d->p->Ntheta+1,
                                d->p->profiles[z_index][M_index]+1,
                                _problems);
            
            if (d->n->monotonize && d->p->is_not_monotonic[z_index][M_index])
            {
                SAFEHMPDF(monotonize(d->p->Ntheta+1,
                                     d->p->is_not_monotonic[z_index][M_index],
                                     _problems, d->p->decr_tgrid,
                                     d->p->profiles[z_index][M_index]+1))
                // check if it worked correctly
                if (!(all_zero(d->p->Ntheta+1, d->p->profiles[z_index][M_index]+1,
                               1e-1*(d->n->signalgrid[1]-d->n->signalgrid[0]))))
                {
                    SAFEHMPDF(not_monotonic(d->p->Ntheta+1,
                                            d->p->profiles[z_index][M_index]+1,
                                            NULL))
                }
                d->p->is_not_monotonic[z_index][M_index] = 0;
            }

            if (d->p->is_not_monotonic[z_index][M_index])
            {
                d->p->is_not_monotonic[z_index][d->n->NM] += 1;
            }
        }
        
    }

    d->p->created_monotonicity = 1;

    ENDFCT
}//}}}

int
create_segments(hmpdf_obj *d)
{//{{{
    STARTFCT

    if (d->p->created_segments) { return hmpdf_status; }

    HMPDFPRINT(2, "\tcreate_segments\n")

    SAFEALLOC(, d->p->segment_boundaries,
              malloc(d->n->Nz * sizeof(int **)))
    #ifdef _OPENMP
    #pragma omp parallel for num_threads(d->Ncores)
    #endif
    for (int z_index=0; z_index<d->n->Nz; z_index++)
    {
        if (hmpdf_status) { continue; }
        SAFEALLOC_NORETURN(, d->p->segment_boundaries[z_index],
                           malloc(d->n->NM * sizeof(int *)))
        if (hmpdf_status) { continue; }
        for (int M_index=0; M_index<d->n->NM; M_index++)
        {
            if (hmpdf_status) { continue; }
            int len = 10;
            SAFEALLOC_NORETURN(, d->p->segment_boundaries[z_index][M_index],
                               malloc(len * sizeof(int)))
            if (hmpdf_status) { continue; }
            
            // 0th element stores the number of segments, it's at least one
            d->p->segment_boundaries[z_index][M_index][0] = 1;

            // the first segment starts at the radial cut-off
            d->p->segment_boundaries[z_index][M_index][1] = 1;

            for (int ii=1; ii<d->p->Ntheta; ii++)
            {
                int sgn_lo = GSL_SIGN(d->p->profiles[z_index][M_index][ii+1]
                                      - d->p->profiles[z_index][M_index][ii]);
                int sgn_hi = GSL_SIGN(d->p->profiles[z_index][M_index][ii+2]
                                      - d->p->profiles[z_index][M_index][ii+1]);
                if (sgn_lo != sgn_hi) // change in gradient
                {
                    ++(d->p->segment_boundaries[z_index][M_index][0]);
                    
                    // check if we still have enough space
                    if (d->p->segment_boundaries[z_index][M_index][0]+2 >= len)
                    {
                        len *= 2;
                        SAFEALLOC_NORETURN(, d->p->segment_boundaries[z_index][M_index],
                                           realloc(d->p->segment_boundaries[z_index][M_index],
                                           len * sizeof(int)))
                        if (hmpdf_status) { continue; }
                    }
                    d->p->segment_boundaries[z_index][M_index]
                        [d->p->segment_boundaries[z_index][M_index][0]] = ii + 1;

                    // the sign of the segment lower ends encodes the gradient
                    d->p->segment_boundaries[z_index][M_index]
                        [d->p->segment_boundaries[z_index][M_index][0]-1] *= sgn_lo;
                }
            }

            // the last segment ends at the cluster centre
            d->p->segment_boundaries[z_index][M_index]
                [d->p->segment_boundaries[z_index][M_index][0]+1] = d->p->Ntheta + 1;
            // get sign for last segment correct
            d->p->segment_boundaries[z_index][M_index]
                [d->p->segment_boundaries[z_index][M_index][0]]
                *= GSL_SIGN(d->p->profiles[z_index][M_index][d->p->Ntheta + 1]
                            - d->p->profiles[z_index][M_index][d->p->Ntheta]);

            // TODO test
            int broken = 0;
            for (int ii=1; ii<d->p->segment_boundaries[z_index][M_index][0]; ii++)
            {
                if (GSL_SIGN(d->p->segment_boundaries[z_index][M_index][ii])
                    == GSL_SIGN(d->p->segment_boundaries[z_index][M_index][ii+1]))
                {
                    broken = 1;
                    printf("not consistent in %d %d\n", z_index, M_index);
                    break;
//                    HMPDFERR_NORETURN("invalid boundaries in %d %d\n", z_index, M_index);
                }
            }
            if (broken)
            {
                for (int ii=0; ii<=d->p->segment_boundaries[z_index][M_index][0]; ii++)
                {
                    printf("\t%d\n", d->p->segment_boundaries[z_index][M_index][ii+1]);
                }
            }
        }
    }

    d->p->created_segments = 1;

    ENDFCT
}//}}}

int
s_of_t(hmpdf_obj *d, int z_index, int M_index, int Nt, double *t, double *s)
// returns signal(t) at z_index, M_index
// TODO thread safety : one accelerator per core!
{//{{{
    STARTFCT

    SAFEALLOC(double *, temp, malloc((d->p->Ntheta+1) * sizeof(double)))
    reverse(d->p->Ntheta+1, d->p->profiles[z_index][M_index]+1, temp);
    interp1d *interp;
    SAFEHMPDF(new_interp1d(d->p->Ntheta+1, d->p->incr_tgrid, temp, temp[0], 0.0,
                           PRINTERP_TYPE, d->p->incr_tgrid_accel[this_core()], &interp))

    for (int ii=0; ii<Nt; ii++)
    {
        SAFEHMPDF(interp1d_eval(interp, t[ii], s+ii))
    }
    free(temp);
    delete_interp1d(interp);

    ENDFCT
}//}}}

int
s_of_ell(hmpdf_obj *d, int z_index, int M_index, int Nell, double *ell, double *s)
// returns the conjugate space profile at z_index, M_index,
//    interpolated at the ell-values passed
{//{{{
    STARTFCT

    interp1d *interp;
    SAFEHMPDF(new_interp1d(d->p->Ntheta, d->p->reci_tgrid,
                           d->p->conj_profiles[z_index][M_index]+1,
                           d->p->conj_profiles[z_index][M_index][1]/*low l*/, 0.0/*high l*/,
                           SELL_INTERP_TYPE, d->p->reci_tgrid_accel, &interp))
    double hankel_norm = 2.0 * M_PI * gsl_pow_2(d->p->profiles[z_index][M_index][0]);
    for (int ii=0; ii<Nell; ii++)
    {
        double l = ell[ii] / d->p->conj_profiles[z_index][M_index][0];
        SAFEHMPDF(interp1d_eval(interp, l, s+ii))
        s[ii] *= hankel_norm;
    }
    delete_interp1d(interp);

    ENDFCT
}//}}}

static int
choose_ordinate(hmpdf_obj *d, int end, int start, inv_profile_e mode, int sgn, double **out)
{//{{{
    STARTFCT

    int indx;
    switch (sgn)
    {
        case (1)  : indx = start - 1;
                    break;
        case (-1) : indx = d->p->Ntheta + 1 - end;
                    break;
        default   : HMPDFERR("unknown sign")
    }

    double *ptr;
    switch (mode)
    {
        case (dtsq_of_s) :
            switch (sgn)
            {
                case (1)  : ptr = d->p->decr_tsqgrid;
                            break;
                case (-1) : ptr = d->p->incr_tsqgrid;
                            break;
            }
            break;
        case (t_of_s)    :
            switch (sgn)
            {
                case (1)  : ptr = d->p->decr_tgrid;
                            break;
                case (-1) : ptr = d->p->incr_tgrid;
                            break;
            }
            break;
        default          : HMPDFERR("unknown mode")
    }

    ptr += indx;

    memcpy(*out, ptr, (end-start) * sizeof(double));

    ENDFCT
}//}}}

static int
remove_duplicates(int *N, double *x, double *y, double eps)
// removes duplicates in y and adjusts x accordingly
//     values are considered equal if they fall within eps of one another
{//{{{
    STARTFCT

    for (int ii=1; ii<*N; ii++)
    {
        if (fabs(y[ii] - y[ii-1]) < eps)
        {
            memmove(y+ii-1, y+ii, (*N - ii)*sizeof(double));
            memmove(x+ii-1, x+ii, (*N - ii)*sizeof(double));
            --*N;
            --ii;
        }
    }

    ENDFCT
}//}}}

static int
update_batches(batch_container_t *b, int inrange, int *inbatch,
               int len_this_batch, int sgn, int ii, double val)
{//{{{
    STARTFCT

        if (!(inrange))
        {
            *inbatch = 0;
        }
        else
        {
            if (!(*inbatch))
            // need to start a new batch
            {
                if (b->Nbatches == 0)
                {
                    SAFEALLOC(, b->batches,
                              malloc(sizeof(batch_t)))
                }
                else
                {
                    SAFEALLOC(, b->batches,
                              realloc(b->batches, (b->Nbatches+1) * sizeof(batch_t)))
                }
                ++b->Nbatches;
                b->batches[b->Nbatches-1].incr = sgn;
                b->batches[b->Nbatches-1].start = ii;
                b->batches[b->Nbatches-1].len = 0;
                SAFEALLOC(, b->batches[b->Nbatches-1].data,
                          malloc(len_this_batch * sizeof(double)));
            }
            b->batches[b->Nbatches-1].data[b->batches[b->Nbatches-1].len] = val;
            ++b->batches[b->Nbatches-1].len;
            *inbatch = 1;
        }

    ENDFCT
}//}}}

void
delete_batch_container(batch_container_t *b)
{//{{{
    for (int ii=0; ii<b->Nbatches; ii++)
    {
        free(b->batches[ii].data);
    }
}//}}}

int
inv_profile(hmpdf_obj *d, int z_index, int M_index, int segment,
            inv_profile_e mode, batch_container_t *b)
// write dtheta^2(signal)/dsignal*dsignal into return values
{//{{{
    STARTFCT

    b->Nbatches = 0;

    int end   = abs(d->p->segment_boundaries[z_index][M_index][segment+2]);
    int start = abs(d->p->segment_boundaries[z_index][M_index][segment+1]);
    int len = end - start;
    int sgn = GSL_SIGN(d->p->segment_boundaries[z_index][M_index][segment+1]);

    // check if there is anything interesting here
    if (all_zero(len, d->p->profiles[z_index][M_index]+start,
                 1e-1*(d->n->signalgrid[1]-d->n->signalgrid[0]))
        || (len < 5))
    {
        return hmpdf_status;
    }

    double *temp;
    SAFEALLOC(, temp, malloc(len * sizeof(double)))
    if (sgn == 1)
    {
        memcpy(temp, d->p->profiles[z_index][M_index]+start,
               len * sizeof(double));
    }
    else
    {
        reverse(len, d->p->profiles[z_index][M_index]+start, temp);
    }

    SAFEALLOC(double *, ordinate, malloc(len * sizeof(double)))
    SAFEHMPDF(choose_ordinate(d, end, start, mode, sgn, &ordinate))

    // check if there are duplicates in temp
    SAFEHMPDF(remove_duplicates(&len, ordinate, temp,
              1e-3 * (d->n->signalgrid[1] - d->n->signalgrid[0])))
    // check if it's too short now
    if (len < 5)
    {
        return hmpdf_status;
    }

    interp1d *interp;
    SAFEHMPDF(new_interp1d(len, temp, ordinate,
                           0.0, 0.0, PRINTERP_TYPE, NULL, &interp))

    // auxiliary variables to keep track of current state
    int inbatch = 0;
    int len_this_batch = (int)(fabs(temp[len-1]-temp[0])
                               / (d->n->signalgrid[1]-d->n->signalgrid[0])) + 4;

    for (int ii = (sgn==1) ? 0 : d->n->Nsignal-1;
         (sgn==1) ? ii<d->n->Nsignal : ii>=0;
         ii += sgn)
    {
        double val;
        int inrange;
        switch (mode)
        {
            case (dtsq_of_s) :
                SAFEHMPDF(interp1d_eval_deriv1(interp, d->n->signalgrid[ii],
                                               &inrange, &val))
                val *= gsl_pow_2(d->p->profiles[z_index][M_index][0])
                       * (d->n->signalgrid[1] - d->n->signalgrid[0]);
                break;
            case (t_of_s)    :
                SAFEHMPDF(interp1d_eval1(interp, d->n->signalgrid[ii],
                                         &inrange, &val))
                val *= d->p->profiles[z_index][M_index][0];
                break;
        }
        SAFEHMPDF(update_batches(b, inrange, &inbatch, len_this_batch,
                                 sgn, ii, val))
    }
    delete_interp1d(interp);

    free(temp);
    free(ordinate);

    ENDFCT
}//}}}

int
init_profiles(hmpdf_obj *d)
{//{{{
    STARTFCT

    if (d->p->inited_profiles) { return hmpdf_status; }

    HMPDFPRINT(1, "init_profiles\n")

    SAFEHMPDF(create_angle_grids(d))
    SAFEHMPDF(create_profiles(d))

    d->p->inited_profiles = 1;

    ENDFCT
}//}}}

