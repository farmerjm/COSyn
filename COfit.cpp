#include "COfit.h"
#include <iostream>
#include <cstdlib>
//optimizations so far:  read in files ONCE in FitData() rather than in each iteration
//                       removal of redundant declarations
//                       memory allocation of rate_eqtn variables in access order
//                       adoption of an optimized linear algebra computation library (armadillo)


//To do:  Fix transposition (later)
//        benchmark rate_eqtn solving
//        add XC180
//        draw spectrum
//        properly loop coll_loop; copy static vars of rate_eqtn into FitData and clone for each loop
//        Properly skip fluorcalc, etc
//        add input file
//        fix g-factors
//

using namespace std;
using namespace idlarma;

FitData* data;

constexpr double FitData::vib_einA[];
constexpr double FitData::Bv12[];
constexpr double FitData::Bv13[];

int FitData::numGuesses;

//returns an array of size numGuesses filled with random numbers on the interval (offset, modulus+offset)
double* FitData::FillArray(int modulus, int offset)
{
  double* array = new double[numGuesses];
  for(int i=0; i<numGuesses;i++) {
    array[i]=rand() % modulus + offset;
  }
  return array;
}

int FitData::runTrial() {
//fit model, compare, etc...
//ranulli is the port of the IDL variable r--fix current r variable?


//Define variables
/*HD100546_luminosity = HD100546_luminosity*L;  //Luminosity scaling
mat fAX = (3.038/2.03)*EinA/wavenum^2
mat fXA = 2*fAX*/

//============================
//Divide into annulli
//=============================
  

  HD100546_luminosity = HD100546_luminosity*rel_lum;
  ranulli.push_back(disk_in); 
  if (r<0.1)
  {
    while ((ranulli.back() < 1) && (ranulli.back() < disk_out))
    {
      r_index_a=r_index_a+1;
      ranulli.push_back(disk_in+0.01*r_index_a);
    }
  }

  maxra = ranulli.back();
 
  if ((maxra < 1) && (maxra >= .1))
  {
    while ((ranulli.back() < 1.0) || (ranulli.back() < disk_out))
    {
     r_index_b++;
     ranulli.push_back(maxra+0.1*r_index_b);
    }
  }
  maxrb=ranulli.back();
 
  if ((maxrb <= disk_out) && (maxrb >= 1.0))
  {
    while  (ranulli.back() < disk_out)
    {
      r_index_c++;
      ranulli.push_back(maxrb+1.0*r_index_c);
    }
  }

  cout << "Ranulli: " << ranulli.at(0) << endl;
  steps=ranulli.size();  //check that this size function is correct
  
  vec rdisk= zeros<vec>(steps);
 
  for (int i=0; i < steps; i++ )
  {
    rdisk.at(i)=ranulli.at(i)*1.496E13;
  }
 
  cout << "rdisk:  " << rdisk << endl;
//benchmark here!  Make sure these results are the same from the IDL code and the C++ code!
  


//=========================================
//  CollData setup
//========================================
  CollData* d = new CollData(this->layers, this->steps);
  for (auto i=0; i<steps;i++) {
    for (auto j=0; j<layers;j++) {
      for (auto k=0; k<10;k++) {
        for (auto q=0; q<11; q++)
        {
          d->rate_eqtn(i,0)(k+11,q,j)=einA.at(k,q);//k+11, q,j,i,   k ,q  //why no zero here
        }
      }
    }
  }

  for (auto j=0; j<steps; j++) 
  {
    d->rate_eqtn.at(j,0)(span(1),span(0),span::all).fill(vib_einA[0]);   
    d->rate_eqtn.at(j,0)(span(10),span(10),span::all).fill(-vib_einA[9]); 

    for (auto i=0; i<9; i++)
    {
      d->rate_eqtn.at(j,0)(span(i+11),span(i+11),span::all).fill(-sum(einA.row(i)));
    }
   

    for (int z=0; z<9; z++)
    {
      for (int q=0; q<layers; q++) 
      {
        d->rate_eqtn.at(j,0).at(z+1,z+1,q)=-vib_einA[z];  
        d->rate_eqtn.at(j,0).at(z+2,z+1,q)=vib_einA[z+1]; 
      }
    }
   
    for (int q=0; q<layers; q++)
    {
      d->rate_eqtn.at(j,0).subcube(span(1),span(0),span::all).fill(vib_einA[0]);  //1,0,all,k
      d->rate_eqtn.at(j,0).subcube(span(9),span(9),span::all).fill(-vib_einA[8]);  //9,9,all,k
    }
   
  }

  d->rate_eqtn2=d->rate_eqtn;   //Clone colldata for the second pass without collisions
   
  cerr << "CollData generated." << endl;

  T_rot_fl = T_rot0_fl *pow((1.5E13/rdisk),T_rot_alpha_fl);
  
  ivec T_rot_index=where(T_rot_fl, [] (double datum) {return datum >= 3500;});

//cout << "T_rot_cnt:  " << T_rot_cnt << endl;
cout << "T_rot_fl:  " << T_rot_fl << endl;
cout << "T_rot_index:  " << T_rot_index << endl;
  if (T_rot_index.at(0) != -1) {
    for (auto elem : T_rot_index )
    {
      T_rot_fl[elem]=3500;
    }
  }


  T_rot_cl=T_rot0_cl * pow((1.496E13/rdisk), T_rot_alpha_cl);
  H_den=H_den0 * pow((1.496E13/rdisk),H_den_alpha);
  T_rot_index = where(T_rot_cl, [] (double datum) {return datum > 3500;});
//  T_rot_cnt=T_rot_index.size();
cerr << "T_rot_index:  " << T_rot_index << endl;
  if (T_rot_index.at(0) != -1) {
    for (auto elem : T_rot_index)
    {
      T_rot_cl[elem]=3500;
    }
  }

  cout << "T_rot_cl:" << endl << T_rot_cl << endl;
  cout << "T_rot_fl:" << endl << T_rot_fl << endl;
  cout << "H_den:"  << endl << H_den << endl;

  
 //   CollData d = new CollData(this->layers, this->steps);
//  if (rel_lum <= 1e-3) goto skip_fluorcalc;
//{
//
    vec b_tot = sqrt(2*1.38e-16*6.02e23*T_rot_fl/28 + pow(v_turb,2));



    vec tau = linspace<vec>(0,19.99,2000); 
    vec F_tau=zeros<vec>(2000);
 
    //==============CALCULATE DFDT BY INTEGRAATION USING TRAPEZOID RULE================
    cout << "b_tot" << endl;
    cout << b_tot;
    
    cout << "tau:";
    cout << tau;
 
    cerr << "Integrating...";
    double sum;
    double n = 1000;
    double a = 0;
    double b = 5;

    double delta=(b-a)/n;

    for (int i=0; i<2000; i++) 
    {
      sum=0.5 * ( (1 - exp(-tau(i)*exp(-pow(a,2)))) + (1 - exp(-tau(i)*exp(-pow(b,2)))) );
      for (int j=0; j<n; j++)  
      {
        auto x=a+delta*j;
        sum=sum+(1-exp(-tau(i)*exp(-pow(x,2))));
      }
      sum = sum*delta;
      F_tau(i)=sum;
    }
    
    vec dFdt=deriv(tau,F_tau);

    cout << "F_tau:  " << endl;    
    cout << F_tau << endl;
    cout << "dFdt  :" << endl;
    cout << dFdt << endl;



      cube Nv_coll;
      mat tot_col_fluor;
      mat tot_col_fluor_back;                      //move, perhaps, to colldata?

      cube Nv_nocoll;
      mat tot_col_fluor_nocoll;



    cerr << "Beginning collisional computations...";

//==========================================================
//   BIG COLLISION LOOP
//=========================================================
    for (int coll_loop=0; coll_loop<2; coll_loop++) 
    {
       cerr << "in Coll_Loop :  " << coll_loop << endl;
      mat U;
      mat s;
      mat v;
       
      vec sol;
      ivec Nv_index;

      mat Fuv = HD100546_luminosity / ( 4*3.1415926535897*pow(rdisk[k],2) );

      for (int k=0; k<steps; k++) 
      {

	cerr <<"Steps loop, k=" << k << endl;
       
       //=============COLLISIONS=============

       if (coll_loop != 1)
       {
	 auto k_HCO_dn = (7.57e-15*T_rot_cl[k]/(1-exp(-3084/T_rot_cl[k])))*H_den[k];
	 auto k_HCO_up=k_HCO_dn*exp(-3084/T_rot_cl[k]);
	 cout << "k_HCO_dn:"  << k_HCO_dn << endl;
	 cout << "k_HCO_up:"  << k_HCO_up << endl;

	 d->rate_eqtn.at(k,0).subcube(span(0),span(0),span::all)-=k_HCO_up;
	 d->rate_eqtn.at(k,0).subcube(span(1),span(0),span::all)+=k_HCO_dn;
	 //0,0,all,k ; 1,0,all,k)
	 for (int i=1; i<9; i++) 
	 {
	   d->rate_eqtn.at(k,0).subcube(span(i-1),span(i),span::all)+=k_HCO_up;
	   d->rate_eqtn.at(k,0).subcube(span(i  ),span(i),span::all) = d->rate_eqtn.at(k  ,0).subcube(span(i),span(i),span::all)-k_HCO_dn-k_HCO_up;
	   d->rate_eqtn.at(k,0).subcube(span(i+1),span(i),span::all)+=k_HCO_dn;
	 }
       }

skip_coll:

	 //===================ULTRAVIOLET PUMPING========================

	 for (int j=0; j<layers; j++)
	 {

	   if (j>0)
	   {
	     for (int i=0; i<10; i++)
	     {
	      d->tau_0.subcube(span(i),span::all,span(j))=7.7585e12 * 0.02654 * arma::sum(d->Nv.slice(k).submat(span(0,11),span(0,j)),1) % fXA.submat(span(i),span::all).t() % lam_ang.submat(span(i),span::all).t()*1e-8/(sqrt(3.1415926535897)*b_tot[k]);
	     }
	   }
           auto max_iter=d->tau_0.slice(j).row(0).n_elem;
	   for (auto ii=0; ii<max_iter; ii++)
	   {
	     if (d->tau_0.at(0,ii,j) < 20)
	     {
	       ivec dFdt_0_index=where(tau, [&] (double elem) {return elem == round(d->tau_0.at(0,ii,j)*10)/10;});
	       auto count = dFdt_0_index.size();
	       cout << "dFdt_0_index:"  << dFdt_0_index <<  "  count:  " << count << endl; 
	       if (dFdt_0_index.at(0) != -1)
	       {
	         d->dFdt_0.subcube(span::all,span(ii),span(j)).fill(dFdt.at(dFdt_0_index.at(0)));    //weird line!!!!!
	       }

	     }
	     else 
	     {
	       d->dFdt_0.slice(j).row(ii).fill((1/(2*d->tau_0.at(0,ii,j))*sqrt(log(d->tau_0.at(0,ii,j)))));    //(span::all,span(i),span(j))=1/(2*tau_0.at(0,ii,j))*sqrt(alog(tau_0.at(0,ii,j)));   CHECK THIS WITH SOURCE
	     }
	   }
           cout << "dFdt_0.slice(j):  " << d->dFdt_0.slice(j) << endl;

	   d->dwdn.slice(j)=d->dFdt_0.slice(j)*.02654*2.0 % (lam_ang*1e-4) % (lam_ang*1e-8) % fXA/(sqrt(3.1415926535897)*c*1e5);

cout << "dwdn.slice(j):  " << endl;
cout << d->dwdn.slice(j) << endl;

	   for (int ii=0; ii<10; ii++) 
	   {
	     cout << "ii,j,k (" << ii << "," << j << "," << k << ")" << endl;
             cout << (d->dwdn.slice(j).row(ii) * 3.1415926535897 % Fuv.row(ii) / (hc * wavenum.row(ii))).t() << endl;
	     d->g.at(ii,0).slice(k).col(j) = (d->dwdn.slice(j).row(ii) * 3.1415926535897 % Fuv.row(ii) / (hc * wavenum.row(ii))).t();  //check this to be sure the constants are filling in right...
           } 
        
	   //add in g-factors:
	   double gsum=0;

	   for (int ii=0; ii<10; ii++)
	   { 
	     gsum+=d->g.at(ii).at(0,j,k);
	   }
	   d->rate_eqtn.at(k,0).at(0,0,j)-=gsum;

	   for (int i=1; i<11; i++)
	   {
	     gsum = 0;
	     for (int ii=0; ii<10; ii++)
	     {
	       gsum+=d->g.at(ii).at(i,j,k);
	     }
	     d->rate_eqtn.at(k,0).subcube(span(i),span(i),span(j))-=gsum;
	   }

           
           for (int i=0; i<9; i++)
	   {
	     for (int ii=0; ii<11; ii++)
	     {
	       d->rate_eqtn.at(k,0).at(ii,11+i,j)+=+d->g.at(i,0).at(ii,j,k); //ii 11+i j k
	     }
	   }
	   d->rate_eqtn.at(k,0).at(1,10,j)=0;
	   
	   
	   vec z = zeros<vec>(21);
	   z.at(20)=1;
	     
	   cout << "matrix for solving:" << endl;
	   cout << d->rate_eqtn.at(k,0).slice(j).t();
	   solve(sol,d->rate_eqtn.at(k,0).slice(j).t(),z);
	   d->Nv.slice(k).col(j)= sol;  //check this to be sure the array is filling in the right dimension

           cout << "d->Nv.slice(k).col(j):  " << d->Nv.slice(k).col(j) << endl;
           Nv_index=where(d->Nv.slice(k).col(j), [] (double datum) {return datum < 0;});
	   if (Nv_index.at(0) != -1) 
           {
             for (int jj=0; jj<Nv_index.n_elem; jj++) 
             {
               d->Nv.at(Nv_index[jj],j,k)=0;
             }
           }
           cout << "End of solve loop" << endl;

 	 }
       }

//}
//skip_fluorcalc:
      cerr << "Checking coll_loop" << endl;
      cerr << "COLL LOOP:  " << coll_loop << endl;
      if (coll_loop==0) {
        Nv_coll=d->Nv;
        cerr << "totalDim:  " << endl;
        cerr << totalDim(d->Nv*7.55858e12,2);
        tot_col_fluor = totalDim(d->Nv*7.55858e12,2);      //check all uses of accu/total in teh code!!!!!! dimension specification!
        cerr << "tot_col_fluor:  " <<  tot_col_fluor << endl;
        tot_col_fluor_back=tot_col_fluor;
        d->rate_eqtn=d->rate_eqtn2;
        cerr << "Beginning pass without collisions..." << endl;
      }
      if (coll_loop==1) {
        Nv_nocoll=d->Nv;
        tot_col_fluor_nocoll = totalDim(d->Nv*7.55858e12,2);
      }
      cerr << "Coll loop incrementing...";
    }


//=========================================================================
// Angle of Incidence Correction (tweak for each star--use input file!)
//========================================================================
  cerr << "Correcting for angle of incidence..." << endl;
  vec m_disk=1.5*(5.59647e-10)*sqrt(T_rot_fl / Mstar)%pow(rdisk,0.5);
  vec m_uv = (5.59647e-10)*sqrt(T_rot_fl/Mstar)%sqrt(rdisk);

  vec phi = -atan((m_uv-m_disk)/(1+m_uv%m_disk));
  phi.at(0)=3.1415926535897/2;                         //standardize pi!

  auto xi = tot_col_fluor.n_rows-1;
  auto yi = tot_col_fluor.n_cols-1;
  cerr << "phi:  " << phi << endl;
  cerr << tot_col_fluor_nocoll << endl;
  cerr << "n_rows:  " << tot_col_fluor.n_rows <<endl;
  cerr << "n_rcols:  " << tot_col_fluor.n_cols<<endl;
  cerr << "xi:  " << xi << endl;
  cerr << "yi:  " << yi << endl;

cerr << tot_col_fluor(span(0,xi),span(0)) << endl;
cerr << "phi.at(0):  " << phi.at(0) << endl;


//tot_col_fluor(span(0,xi),span(1))=sin(phi.at(1))*tot_col_fluor(span(0,xi),span(1));
//cerr << tot_col_fluor(span(0,xi),span(1));
  for (int j=0; j<yi; j++)                    //looks like I need to transpose indices on everything involving tot_col_fluor 
  {
 cerr << "Loop dat.  " << j << endl;
    tot_col_fluor(span(0,xi),span(j))=sin(phi.at(j))*tot_col_fluor(span(0,xi),span(j));
 cerr << "And a half.  "  << endl;
    tot_col_fluor_nocoll(span(0,xi),span(j))=sin(phi.at(j))*tot_col_fluor_nocoll(span(0,xi),span(j));
  }
cerr << "Out of loop!" << endl;
  mat tot_col=tot_col_fluor(span(0,9),span::all);
  mat tot_col_coll=tot_col;

  double r = dist/1.496e13;
  rdisk=rdisk/1.496e13;            //convert to AU



//===================================================================================================
//  MOLECULAR DATA
//==================================================================================================
  
  cerr << "Beginning the processing of molecular data..." << endl;

  cube N_12CO_vj = zeros<cube>(tot_col.col(0).n_elem-1,120,tot_col.row(0).n_elem);                 //flipping these indices...
  cube N_13CO_vj = zeros<cube>(3,120,tot_col.row(0).n_elem);
  cube N_C18O_vj = zeros<cube>(1,120,tot_col.row(0).n_elem);

  vec Jup;
  vec Jdn;
  vec wvn;
  vec EinA;

  for (int i=0; i<steps; i++)
  {

    cerr << "steps loop i=" << i << endl;

    //=======================
    //   12CO
    //======================

    cerr << "12CO" << endl;
    cout << "X12CO: " << X12CO << endl; 

cerr << "TEST." << endl;
    cerr << X12CO.n_rows << endl;
    cerr  << X12CO.n_cols << endl; 
    cerr  << X12CO.n_slices << endl;
    cerr << X12CO(span(3),span(0),span::all);

    for (int j=0; j< tot_col.row(0).n_elem-1; j++)
    {
      cerr << j << endl;
      Jup = X12CO(span(3),span(j),span::all);          //because the indices are reversed in c++, do all these first/second indices flipped.
      Jdn = X12CO(span(1),span(j),span::all);
      wvn = X12CO(span(6),span(j),span::all);
      EinA= X12CO(span(4),span(j),span::all);
      cerr << "midway" << endl;
      N_13CO_vj.slice(i).row(j)=tot_col_fluor.at(j+1,i) * (2*Jup+1) % exp(-hck*Bv12[j]*Jup % (Jup+1)/T_rot_fl[i])/(T_rot_fl[i]/(hck*Bv12[j])) + tot_col_coll.at(j+1,i)*(2*Jup+1) % exp(-hck*Bv12[j] * Jup % (Jup+1)/T_rot_cl[i])/(T_rot_cl[i]/(hck*Bv12[j]));
    }
  cerr << "13CO" << endl;
    //====================
    //  13CO
    //====================
    for (int j=0; j<3; j++)
    {
      Jup = X13CO(span(3),span(j),span::all);
      Jdn = X13CO(span(1),span(j),span::all);
      wvn = X13CO(span(6),span(j),span::all);
      EinA= X13CO(span(4),span(j),span::all);
     
      N_13CO_vj.slice(i).row(j)=(tot_col_fluor_nocoll.at(j+1,i)/X12CO_13CO_fl)*(2*Jup+1) * exp(-hck*Bv13[j] * Jup % (Jup+1) / T_rot_fl[i]) / (T_rot_fl[i]/(hck*Bv13[j]));

    }
  cerr << "C18O" << endl;
    //====================
    //  C180
    //===================
    
      Jup = XC18O(span(3),span(0),span::all);
      Jdn = XC18O(span(1),span(0),span::all);
      wvn = XC18O(span(6),span(0),span::all);
      EinA= XC18O(span(4),span(0),span::all);

      N_C18O_vj.slice(i).row(0)=(tot_col_fluor_nocoll.at(1,i)/X12CO_C18O_fl)*(2*Jup+1) * exp(-hck*Bv18[0]*Jup%(Jup+1)/T_rot_fl[i]) / (T_rot_fl[i]/(hck*Bv18[0]));

  }

  
  double freq_size=log10(f_f/f_i)/log10(1+v/(3*c));
  vec freq = zeros<vec>(freq_size);
  freq.at(0)=f_i;
  vec vfreq = freq;
  vfreq.fill(0);

  for (int i=1; i<freq_size; i++)
  {
    freq.at(i)=f_i*(pow(1+v/(3*c),i));
  }

  vfreq=(freq(round(freq.n_elem/2))-freq)*c/freq(round(freq.n_elem/2));                      //check


  ivec freq_index=where(freq, [] (double datum) {return (datum >= f_i) && (datum <= f_f);});

//define annuli

  vec annuli = zeros<vec>(rdisk.n_elem);

  for (int i=0; i<steps-1; i++)
  {
    annuli.at(i)=3.1415926535897*(pow(rdisk.at(i+1),2)-pow(rdisk.at(i),2));
  }
  annuli(steps-1)=3.1415926535897*(pow(rdisk.at(steps-1)+1,2)-pow(rdisk.at(steps-1),2));

  mat stick_spec_12CO = zeros<mat>(freq_size,steps);
  mat stick_spec_13CO = stick_spec_12CO;
  mat stick_spec_C18O=stick_spec_12CO;
  mat stick_spec_tot=stick_spec_12CO;

  double A0;
  double A1;
  double A2;
cerr << "Beginning molecular processing over annuli:" << endl;
  //======================
  //  X12CO
  //=====================
  for (int i=0; i<steps; i++)  //Loop over annuli
  {
   cerr << "annulus i=" << i << endl;
    //==============================
    //  X12CO
    //=============================
    for (int j=0; j<tot_col.row(0).n_elem-1; j++)  //vibrational levels
    {
      Jup = data->X12CO(span(3),span(j),span::all);
      Jdn = data->X12CO(span(1),span(j),span::all);
      wvn = data->X12CO(span(6),span(j),span::all);
      EinA= data->X12CO(span(4),span(j),span::all);
    
      for (int k=0; k<X12CO.n_slices; k++)  //Loop over rotational levels
      {
        if ( (wvn(k) >= f_i) && (wvn(k) <= f_f) )
        {
          A0=N_13CO_vj.at(j,k,i)*hc*wvn(k)*EinA(k);                         //should the first two indices be reversed here?
          A1=wvn(k);
          A2=b_tot(i)*wvn(k)/(c*1e5);
          stick_spec_12CO.col(i)+=(A0/(sqrt(3.1415926535897)*A2)) * exp (pow(-((A1-freq)/A2),2));
        }

      }

    }
 

    //==============================
    //  X13CO
    //==============================
    for (int j=0; j<3; j++)   //Loop over vibrational levels--check for-loop bounds in some earlier integral loops... might need to be adjusted!
    {

      Jup = data->X13CO(span(3),span(j),span::all);
      Jdn = data->X13CO(span(1),span(j),span::all);
      wvn = data->X13CO(span(6),span(j),span::all);
      EinA= data->X13CO(span(4),span(j),span::all);

      for (int k=0; k<X13CO.n_slices; k++)
      {
          A0=N_13CO_vj.at(j,k,i)*hc*wvn(k)*EinA[k];                         //should the first two indices be reversed here?
          A1=wvn[k];
          A2=b_tot[i]*wvn[k]/(c*1e5);
          stick_spec_13CO.col(i)+=(A0/(sqrt(3.1415926535897)*A2)) * exp (pow(-((A1-freq)/A2),2));
      }    

    }

    //=============================
    //  XC180
    //=============================
    
     
      Jup = data->XC18O(span(3),span(0),span::all);
      Jdn = data->XC18O(span(1),span(0),span::all);
      wvn = data->XC18O(span(6),span(0),span::all);
      EinA= data->XC18O(span(4),span(0),span::all);

      for (int k=0; k<XC18O.n_slices; k++)
      {
          A0=N_13CO_vj.at(0,k,i)*hc*wvn[k]*EinA[k];                         //should the first two indices be reversed here?
          A1=wvn[k];
          A2=b_tot[i]*wvn[k]/(c*1e5);
          stick_spec_C18O.col(i)+=(A0/(sqrt(3.1415926535897)*A2)) * exp (pow(-((A1-freq)/A2),2));
      }
  
    stick_spec_tot.row(i)=stick_spec_12CO.row(i)+stick_spec_13CO.row(i)+stick_spec_C18O.row(i);  //Note:  I have the notation backwards... since IDL is backwards, (*,i) is col(i).  Fix all of these!

  }

  annuli.at(0)=1e28;
  mat iten_tot=stick_spec_tot;
  iten_tot.row(0)=iten_tot.row(0)*2.5;
  mat Lum_tot = iten_tot;

  for (int i=0; i<steps; i++)
  {
    Lum_tot.row(i)=iten_tot.row(i)*annuli.at(i);
  }

  mat Flux_tot = Lum_tot/(4*3.1415926535897*pow(data->stardist,2));


//===================================
//  SCRIPT 4 of 4
//====================================

  double r_in=rdisk.at(0);
  double r_out=rdisk.at(rdisk.n_elem-1);

  double dv=1;

  mat flux_tot_slit=Flux_tot;

//account for slit loss
  vec slit_loss=100.17*pow(rdisk,-1.260);
  double disk_Beyond_slit = 63;

  for (int i=0; i<rdisk.n_elem; i++)
  {
    if (rdisk.at(i) > disk_Beyond_slit)
    {
      flux_tot_slit.col(i)=flux_tot_slit.col(i)*slit_loss.at(i);
    }
  }

  auto n_rings=rdisk.n_elem;

  vec dr = rdisk;

  for (int i=0; i<n_rings; i++)
  {
    if (rdisk(i) < 0.1)    {
      dr(i)=0.01;
    } else {
    dr(i)=1.0;
    }
  }

  vec vmax = zeros<vec>(n_rings);
  vec max=round(sqrt(887.2*Mstar/rdisk));

//construct frequency scale!

//  vec vfreq = zeros<vec>(freq.n_elem);
//  vfreq=(freq(round(freq.n_elem/2))-freq)*c/freq(round(freq.n_elem/2));

//  freq_index=where(freq, [] (double datum) {return (freq >= f_i) && (freq <= f_f);});     //these have already been done before.. redundant?

  auto total_spec=iten_tot;
  auto vel_spec=total_spec.col(0);

  return 0;
}








int FitData::runTrials() {
  cerr << "Running trials...";
  //start with best chi-by-eye fit and run one trial
  //then each time, reset parameters to a value from the matrix and rerun.

  //set parameters for best-chi;
  layers=300;
  disk_in=13.;
  dist=1.496e13*disk_in;
  disk_out=100.0;
  v_turb=3e5;
  T_rot0_fl=2500;             //check this.... is t_rot_fl0?
  T_rot_alpha_fl=0.25;
  T_rot0_cl=T_rot0_fl;
  T_rot_alpha_cl=T_rot_alpha_fl;
  rel_lum=20;
  
  this->runTrial();

  for(int i=0; i<this->numGuesses;i++) {
    //set parameters here
    layers=randData[0][i];
    disk_in=randData[1][i];
    dist=1.496e13*disk_in;
    disk_out=randData[2][i];
    v_turb=randData[3][i];
    T_rot0_fl=randData[4][i];
    T_rot_alpha_fl=randData[5][i];
    T_rot0_cl=T_rot0_fl;
    T_rot_alpha_cl=T_rot_alpha_fl;
    rel_lum=randData[6][i];
    this->runTrial();
  }

  return 0;
}

FitData::FitData(int numGuesses)
{
  //class variables

  FitData::numGuesses=numGuesses;



  //read in data from files
  cerr << "Reading in files...";

  std::ifstream fin;

  fin.open("inc/EinA.txt");
  for ( auto& entry : einA) {
     fin >> entry;
  }
  fin.close(); 

  cout << "EinA:  " << einA << endl;

  fin.open("inc/lambda.txt");
  for ( auto& entry : lam_ang) {
     fin >> entry;
  }
  fin.close();


  fin.open("inc/wavenum.txt");
  for ( auto& entry : wavenum) {
     fin >> entry;
  }
  fin.close();
 
  cout << "wavenum:  " << wavenum << endl;

  fin.open("inc/HD100546_luminosity.txt");
  for (auto& entry : HD100546_luminosity) {
     fin >> entry;
  }
  fin.close();


//============================
//     MOLECULAR DATA
//===========================

/*  fin.open("CO_molecdat/X12CO");
  for (int i=0; i<119; i++) {
    for (int j=0; j<7; j++) {
      for (int k=0; k<10; k++) {
        fin >> X12CO(k,j,i);
      }
    }
  }
  fin.close();

  cout << "X12CO:  " << X12CO << endl;

  fin.open("CO_molecdat/X13CO");
  for (int i=0; i<119; i++) {
    for (int j=0; j<7; j++) {
      for (int k=0; k<3; k++) {
         fin >> X13CO(k,j,i);
      }
    }
  }
  fin.close();*/

  mat temp = zeros<mat>(7,10);
  fin.open("CO_molecdat/X12CO");

  for (int k=0; k<119; k++)
  {
    for (int i=0; i<7; i++)
    {
      for (int j=0; j<10; j++)
      {
        fin >> temp(i,j);
      }
    }
    X12CO.slice(k)=temp.t();
  }

  fin.close();

  fin.open("CO_molecdat/X13CO");
  temp = zeros<mat>(7,3);

  for (int k=0; k<119; k++)
  {
    for (int i=0; i<7; i++)
    {
      for (int j=0; j<3; j++)
      {
        fin >> temp(i,j);
      }
    }
    X13CO.slice(k)=temp.t();
  }

  fin.close();

  fin.open("XC180");


  fin.close();



/*//transpose these to get the write dimensinos--eventually transpose instead in host files
  HD100546_luminosity=HD100546_luminosity.t();
  wavenum=wavenum.t();
  einA=einA.t();
  lam_ang=lam_ang.t();

  cout << "HD100546_luminosity:  " << HD100546_luminosity << endl;
  cout << "wavenum:  " << wavenum << endl;
  cout << "EinA:  " << einA << endl;
  cout << "lam_ang:  " << lam_ang << endl;
 
//figure out how to deal with these... might just have to access them in a different order, or reverse the cube slicing?
  cout << "X12CO:  " << X12CO << endl;
  cout << "X13CO:  " << X13CO << endl;*/



//initialize arrays with random data
  cerr << "Generating arrays...";
  this->randData[0]=FillArray(900, 100);
  this->randData[1]=FillArray(15, 6);
  this->randData[2]=FillArray(76, 50);
  this->randData[3]=FillArray(500000, 100000);      //check to be sure all of these first arguments don't need to be offset by +/- 1
  this->randData[4]=FillArray(3500,1000);
  this->randData[5]=FillArray(50,20);
  this->randData[6]=FillArray(99, 1);
  for (int i=0; i <= numGuesses; i++) {
    this->randData[5][i]=this->randData[5][i]/100;
  };


 //print arrays for debugging purposes
  for (int c=0; c<7; c++)
  {
    cout << "randData[" << c << "]:  ";
    for (int i=0; i<numGuesses; i++) {
      cout << this->randData[c][i] << ' ' ;
    }
  };
 
 /*=================
 *
 * Collision Data                                      EVERYTHING AFTER LAYERS ----> INTO COLLISIONS!  CollisionData.cpp will need to be revisited...
 *
 *==================*/
  cerr << "Preparing collision data." << endl;
  fAX = (3.038/2.03)*einA/(wavenum % wavenum);   //be sure this is right!
  fXA = 2*fAX;

  this->runTrials();
}

FitData::~FitData()
{
  for(int i=0; i<=7;i++)
  {
    delete this->randData[i];
  }
  delete this->randData;
}

int createInput() 
{
  

  return 1;
} 

int readInput(string inpFile)
{

  return 1;
}

int main(int argc, char* argv[]) 
{
  cout << "Begin!" << endl;
  data = new FitData(atoi(argv[1]));
  data->runTrials();
  delete data;
  return 1;
}
