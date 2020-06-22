/*! @file hmpdf_powerspectrum.h */
#ifndef HMPDF_POWERSPECTRUM_H
#define HMPDF_POWERSPECTRUM_H

/*! Power spectrum/correlation function output modes. */
typedef enum
{
    hmpdf_onehalo, /*!< output only the 1-halo term */
    hmpdf_twohalo, /*!< output only the 2-halo term */
    hmpdf_total,   /*!< output sum of both */
} hmpdf_Cell_mode_e;

/*! Returns the angular power spectrum.
 *
 *  \param d        hmpdf_init() must have been called on d
 *  \param Nell     number of ell-values the power spectrum is to be output at
 *  \param ell      array of length Nell
 *  \param Cell     output array, at least Nell long
 *  \param mode     one of #hmpdf_Cell_mode_e
 *  \return void
 */
int hmpdf_get_Cell(hmpdf_obj *d,
                   int Nell,
                   double *ell,
                   double *Cell,
                   hmpdf_Cell_mode_e mode);

/*! Returns the angular correlation function.
 *
 *  \param d        hmpdf_init() must have been called on d
 *  \param Nphi     number of phi-values the correlation function is to be output at
 *  \param phi      array of length Nphi
 *  \param Cphi     output array, at least Nphi long
 *  \param mode     one of #hmpdf_Cell_mode_e
 *  \return void
 */
int hmpdf_get_Cphi(hmpdf_obj *d,
                   int Nphi,
                   double *phi,
                   double *Cphi,
                   hmpdf_Cell_mode_e mode);


#endif
