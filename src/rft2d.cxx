#include "rft2d.h"
#include <iostream>
#include <fstream>
#include <cmath> 

RFT::iCDF::iCDF(int N_, double fluct_)
:	N(N_), fluct(fluct_)
{
	// minimum of the centered unit variance gaussian variable
	y_gamma_cdf = new double[N];
	x_gamma = new double[N];
	dx_gamma = 10.*sqrt(fluct)/N;
	for (int i=0;i<N;i++)
	{
		x_gamma[i] =  i*dx_gamma;
		y_gamma_cdf[i] = gsl_sf_gamma_inc_P(fluct, x_gamma[i]);

		
	}
	acc = gsl_interp_accel_alloc();
    spline = gsl_spline_alloc(gsl_interp_linear, N);
    gsl_spline_init(spline, y_gamma_cdf, x_gamma, N);
}
RFT::iCDF::~iCDF()
{
	gsl_spline_free(spline);
    gsl_interp_accel_free(acc);
}
double RFT::iCDF::operator()(double gaussian_X)
{
	double cdf = 1. - gsl_sf_erf_Q(gaussian_X), result;
	if (cdf < y_gamma_cdf[0])
	{
		return x_gamma[0]/fluct;
	}
	else if (cdf > y_gamma_cdf[N-1])
	{
		return x_gamma[N-1]/fluct;
	}
	else
	{
		result = gsl_spline_eval(spline, cdf, acc)/fluct;
		if (result < 0.0)
		{
			return 0.0;
		}
		else
			return result;
	}
}

//-----------------------------

RFT::rft2d::rft2d(int const N1_, int const N2_, double L1_, double L2_, double Var_Phi_, double lx_, int seed_, double width_)
: 	N1(N1_), N2(N2_), 
	L1(L1_), L2(L2_), 
	Var_x(Var_Phi_), Var_k(sqrt(M_PI*2*lx_*lx_/N1_/N2_/L1_/L2_)), 
	lx(lx_), coeff_k(-M_PI*M_PI*2.0*lx_*lx_),
	dx1(L1_/double(N1_)), dx2(L2_/double(N2_)), 
	dk1(1./L1), dk2(1./L2), 
	white_noise_generator(0.0, 1.0),
	icdf(500, Var_Phi_),
	width(width_),
	dxy2(L1_*L2_/N1_/N2_),
	dxy(L1_/N1_),
	Ncut(int(3.0*width_*N1_/L1_))
{
	//----------------Set up white generator-------------------
	generator.seed(seed_);
	//----------------FFTW-------------------------------------
	phi_x = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N1*N2);
	phi_k = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N1*N2);
	//std::cout << "creating FFT plans" << std::endl;
	plan_k2x = fftw_plan_dft_2d(N1, N2, phi_k, phi_x, FFTW_BACKWARD, FFTW_MEASURE);
	plan_x2k = fftw_plan_dft_2d(N1, N2, phi_x, phi_k, FFTW_FORWARD, FFTW_MEASURE);
	//std::cout << "FFT plans finished" << std::endl;
	//---------------Set up TAB clip---------------------------
	TAB_clip = new double*[Ncut*2+1];
	int ic, jc;
	for(int i=0; i< Ncut*2+1; i++)
	{
		TAB_clip[i] = new double[Ncut*2+1];
		for(int j=0; j< Ncut*2+1; j++)
		{
			ic = i - Ncut;
			jc = j - Ncut;
			TAB_clip[i][j] = std::exp(-(ic*ic+jc*jc)*dxy2/width/width)*dxy2/(M_PI*width*width);
		}
	}
}

RFT::rft2d::~rft2d()
{
	fftw_destroy_plan(plan_k2x);
	fftw_destroy_plan(plan_x2k);
}

void RFT::rft2d::real_space_white_noise(void)
{
	for (int i = 0; i < N1; i++)
	{
		for (int j = 0; j < N2; j++)
		{
			phi_x[i*N1+j][0] = white_noise_generator(generator); // real white noise source in x-space
			phi_x[i*N1+j][1] = 0.0; // no imag part in x-space
		} 
	}
}

void RFT::rft2d::apply_k_spcae_propagation(void)
{
	double ker, nr2;
	double si, sj, re, im;
	for (int i = 0; i < N1; i++)
	{
		for (int j = 0; j < N2; j++)
		{
			si = std::min(i,N1-i)/L1;
			sj = std::min(j,N2-j)/L2;
			nr2 = 0.5*coeff_k*(pow(si, 2) + pow(sj, 2));
			ker = Var_k*exp(nr2);
			phi_k[i*N1+j][0] *= ker; 
			phi_k[i*N1+j][1] *= ker; 
		} 
	}
}

void RFT::rft2d::run()
{ 
	real_space_white_noise();

	fftw_execute(plan_x2k);
	apply_k_spcae_propagation();
	fftw_execute(plan_k2x);
}


