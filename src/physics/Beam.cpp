//---------------------------------------------------------------------------


#pragma hdrstop

#include "Beam.h"
// #include "Types.h"
//---------------------------------------------------------------------------
__fastcall TBeam::TBeam(int N)
{
    Np=N;
    Nbars=200;
    Kernel=0.95;
    lmb=1;
    Cmag=0;
    SetKernel(Kernel);
    
    Particle=new TParticle[Np];
    for (int i=0;i<Np;i++)
        Particle[i].lost=LIVE;

    Nliv=N;
    Reverse=false;
}
//---------------------------------------------------------------------------
__fastcall TBeam::~TBeam()
{
    delete[] Particle;
}
//---------------------------------------------------------------------------
void TBeam::SetBarsNumber(int N)
{
    Nbars=N;
}
//---------------------------------------------------------------------------
void TBeam::SetKernel(double Ker)
{
    Kernel=Ker;
    h=1;//sqrt(-2*ln(1-Kernel));   ALWAYS RMS!
}
//---------------------------------------------------------------------------
//MOVED TO TYPES.H
/*void TBeam::TwoRandomGauss(double& x1,double& x2)
{
	double Ran1=0;
    double Ran2=0;

    Ran1=((double)rand() / ((double)RAND_MAX + 1));
    Ran2=((double)rand() / ((double)RAND_MAX + 1));

    if (Ran1<1e-5)
        Ran1=1e-5;
        
    x1=sqrt(-2*ln(Ran1))*cos(2*pi*Ran2);
    x2=sqrt(-2*ln(Ran1))*sin(2*pi*Ran2);
}        */
//---------------------------------------------------------------------------
//REMOVED
/*int TBeam::CountCSTParticles(AnsiString &F)
{
	return NumPointsInFile(F,2);
}     */
//---------------------------------------------------------------------------
void TBeam::GenerateEnergy(TGauss G)
{
	MakeGaussDistribution(G,BETA_PAR);
}
//---------------------------------------------------------------------------
void TBeam::GeneratePhase(TGauss G)
{
	MakeGaussDistribution(G,PHI_PAR);
}
//---------------------------------------------------------------------------
void TBeam::GenerateAzimuth(TGauss G)
{
	MakeGaussDistribution(G,TH_PAR);
}
//---------------------------------------------------------------------------
double **TBeam::ImportFromFile(TBeamType BeamType,TBeamInput *BeamPar,bool T)
{
	AnsiString F,L,S;
	int LineLength=-1,i=0;
	bool Success=false;
	double **X;

	switch (BeamType){
		case CST_PIT:{
			F=BeamPar->RFile;
			LineLength=PIT_LENGTH;
			break;
		}
		case CST_PID:{
			F=BeamPar->RFile;
			LineLength=PID_LENGTH;
			break;
		}
		case FILE_1D:{
			F=BeamPar->ZFile;
			LineLength=1;
			break;
		}
		case FILE_2D:{
			F=T?BeamPar->RFile:BeamPar->ZFile;
			LineLength=2;
			break;
		}
		case TWO_FILES_2D:{
			F=BeamPar->YFile;
			LineLength=2;
			break;
		}
		case FILE_4D:{
			F=BeamPar->RFile;
			LineLength=4;
			break;
		}
		default: {break;}
	}

	std::ifstream fs(F.c_str());
	X=new double *[LineLength];
	for (int j = 0; j < LineLength; j++) {
		X[j]= new double [BeamPar->NParticles];
	}

	while (!fs.eof()){
		L=GetLine(fs);

		if (NumWords(L)==LineLength){       //Check the proper numbers of words in the line
			try {
				for (int j=1;j<=LineLength;j++){
					//Check if the data is really a number
					S=ReadWord(L,j);
					X[j-1][i]=S.ToDouble();
				}
			}
			catch (...){
				continue;          //Skip incorrect line
			}
			if (i==BeamPar->NParticles){     //if there is more data than expected
			  //	i++;
				break;
			}
			i++;
		}
	}
	fs.close();

	Success=(i==BeamPar->NParticles);

	if (!Success)
		X=DeleteDoubleArray(X,LineLength);

	return X;
}
//---------------------------------------------------------------------------
bool TBeam::ImportEnergy(TBeamInput *BeamPar)
{
	double **X=NULL;
	double phi=0,W=0;
	int i=0;
	bool Success=false;

	X=ImportFromFile(BeamPar->ZBeamType,BeamPar,false);

	if (X!=NULL) {
		for (int i = 0; i < BeamPar->NParticles; i++) {
			switch (BeamPar->ZBeamType){
				case FILE_1D:
				{
					Particle[i].beta=MeVToVelocity(X[0][i]);
					break;
				}
				case FILE_2D:
				{
					Particle[i].phi=DegreeToRad(X[0][i]);
					Particle[i].beta=MeVToVelocity(X[1][i]);
					break;
				}
				default: {};
			}
		}
		int N=0;
		switch (BeamPar->ZBeamType){
			case FILE_1D: {N=1;break;}
			case FILE_2D: {N=2;break;}
		}
		X=DeleteDoubleArray(X,N);
		Success=true;
	} else
		Success=false;

	return Success;
}
//---------------------------------------------------------------------------
bool TBeam::BeamFromCST(TBeamInput *BeamPar)
{
	double **X=NULL;
	double x=0,y=0,z=0,px=0,py=0,pz=0,t=0;
	double r=0,th=0,p=0,pr=0,pth=0,beta=0;
	int i=0;
	bool Success=false;
	TPhaseSpace C;

   /*FILE *logFile;
	logFile=fopen("BeamImport.log","w");    */

	X=ImportFromFile(BeamPar->RBeamType,BeamPar,true);

	if (X!=NULL) {
		for (int i = 0; i < BeamPar->NParticles; i++) {
			switch (BeamPar->RBeamType){
				case CST_PIT:
				{
					t=X[9][i];
					Particle[i].phi=-2*pi*t*c/lmb;
					if (BeamPar->ZCompress){
						int phase_dig=-DigitConst*Particle[i].phi;
						int max_dig=DigitConst*2*pi;
						int phase_trunc=phase_dig%max_dig;
						Particle[i].phi=(1.0*phase_trunc)/DigitConst;
					}
				}
				case CST_PID:
				{
					x=X[0][i];
					y=X[1][i];
					z=X[2][i];
					px=X[3][i];
					py=X[4][i];
					pz=X[5][i];

					C=CartesianToCylinrical(x,y,px,py);
					r=C.x;
					th=C.y;
					pr=C.px;
					pth=C.py;

					p=sqrt(px*px+py*py+pz*pz);
					beta=1/sqrt(1+1/sqr(p));

					Particle[i].beta=beta;
					Particle[i].Bz=(pz/p)*beta;
					//Particle[i].r=x/lmb;
					Particle[i].r=r/lmb;
					//Particle[i].Br=(px/p)*beta;
					Particle[i].Br=(pr/p)*beta;
					Particle[i].Th=th;
					Particle[i].Bth=(pth/p)*beta;

					//fprintf(logFile,"%f %f %f\n",px,pr*cos(th)-pth*sin(th),px-pr*cos(th)+pth*sin(th));
				}
				default: {};
			}
		}
		int N=0;
		switch (BeamPar->RBeamType){
			case CST_PIT: {N=PIT_LENGTH;break;}
			case CST_PID: {N=PID_LENGTH;break;}
		}
		X=DeleteDoubleArray(X,N);
		Success=true;
	} else
		Success=false;

	//fclose(logFile);

	return Success;
}
//---------------------------------------------------------------------------
bool TBeam::BeamFromTwiss(TBeamInput *BeamPar)
{
	TPhaseSpace *X,*Y, C;
	double x=0,y=0,px=0,py=0,pr=0,pth=0,r=0,th=0;

	X=MakeTwissDistribution(BeamPar->XTwiss);
	Y=MakeTwissDistribution(BeamPar->YTwiss);

	//FILE *logFile;
	//logFile=fopen("TwissGen.log","w");

	for (int i=0; i < Np; i++) {
		x=X[i].x;
		y=Y[i].x;
		px=tg(X[i].px);
		py=tg(Y[i].px);

		C=CartesianToCylinrical(x,y,px,py);
		r=C.x;
		th=C.y;
		pr=C.px;
		pth=C.py;

		//fprintf(logFile,"%f %f %f %f %f %f %f %f\n",x,y,px,py,C.x,C.y,C.px,C.py);
	   //	fprintf(logFile,"%f %f\n",x,px);

		Particle[i].r=r/lmb;
		Particle[i].Br=pr*Particle[i].beta;
		Particle[i].Th=th;
		Particle[i].Bth=pth*Particle[i].beta;//0;
		//CHECK sqrt(negative)!
		Particle[i].Bz=BzFromOther(Particle[i].beta,Particle[i].Br,Particle[i].Bth);//double-check!!!

		//fprintf(logFile,"%f %f\n",Particle[i].r,Particle[i].Br);
	}

   //	fclose(logFile);

	delete[] X;
	delete[] Y;

	return true;
}
//---------------------------------------------------------------------------
bool TBeam::BeamFromFile(TBeamInput *BeamPar)
{
	double **X=NULL, **Y=NULL;
	double x=0,y=0,px=0,py=0;
	double r=0,th=0,p=0,pr=0,pth=0,beta=0;
	int i=0;
	bool Success=false,Full4D=true;
	TPhaseSpace C;

	X=ImportFromFile(BeamPar->RBeamType,BeamPar,true);
	if (BeamPar->RBeamType==TWO_FILES_2D)
		Y=ImportFromFile(FILE_2D,BeamPar,true);
	else
		Y=X;

	if (X!=NULL && Y!=NULL) {
		for (int i = 0; i < BeamPar->NParticles; i++) {
			switch (BeamPar->RBeamType){
				case FILE_2D:
				{
					x=mod(X[0][i]);
                    //x=X[0][i];
					px=X[1][i];
					Particle[i].r=0.01*x/lmb;
					Particle[i].Br=px*Particle[i].beta;
					//Particle[i].Th=0;
					Particle[i].Bth=0;
					Full4D=false;
					break;
				}
				case TWO_FILES_2D:
				{
					x=Y[0][i];
					px=Y[1][i];
					y=X[0][i];
					py=X[1][i];
					break;

				}
				case FILE_4D:
				{
					x=X[0][i];
					px=X[1][i];
					y=X[2][i];
					py=X[3][i];
					break;
				}
				default: {};
			}
			if (Full4D) {
				C=CartesianToCylinrical(x,y,px,py);
				r=C.x;
				th=C.y;
				pr=C.px;
				pth=C.py;

				Particle[i].r=0.01*r/lmb;
				Particle[i].Br=pr*Particle[i].beta;
				Particle[i].Th=th;
				Particle[i].Bth=pth*Particle[i].beta; //check! debugging may be required
			}
			//CHECK sqrt(negative)!
			Particle[i].Bz=BzFromOther(Particle[i].beta,Particle[i].Br,Particle[i].Bth);//double-check!!!
		}
		int N=0;
		switch (BeamPar->RBeamType){
			case FILE_2D: {N=2;break;}
			case TWO_FILES_2D: {N=2;Y=DeleteDoubleArray(Y,N);break;}
			case FILE_4D: {N=4;break;}
		}
		X=DeleteDoubleArray(X,N);
		Success=true;
	} else
		Success=false;

	return Success;
}
//---------------------------------------------------------------------------
bool TBeam::BeamFromSphere(TBeamInput *BeamPar)
{
	double *X, *Y;
	double r=0,th=0,pr=0,pz=0,v=0,psi=0;
	TGauss Gx, Gy;
	TPhaseSpace C;

	Gx.mean=0;
	Gx.limit=BeamPar->Sph.Rcath;
	Gx.sigma=BeamPar->Sph.Rcath;
	Gy.mean=0;
	Gy.limit=pi;
	Gy.sigma=100*pi;

	X=MakeRayleighDistribution(Gx);
	Y=MakeGaussDistribution(Gy);

	for (int i=0; i < Np; i++) {
		r=X[i];
		th=Y[i];

		pr=BeamPar->Sph.Rsph==0?0:-sin(r/BeamPar->Sph.Rsph);
		//pz=BeamPar->Sph.Rsph==1?0:-cos(r/BeamPar->Sph.Rsph);

		if (BeamPar->Sph.kT>0) {
			v=sqrt(2*qe*BeamPar->Sph.kT/me);
			psi=RandomCos();
			pr+=v*cos(psi)/(Particle[i].beta*c);
		}

		Particle[i].r=r/lmb;
		Particle[i].Br=pr*Particle[i].beta;
		Particle[i].Th=th;
		Particle[i].Bth=0;
		Particle[i].Bz=BzFromOther(Particle[i].beta,Particle[i].Br,0);//double-check!!!
    }

	delete[] X;
	delete[] Y;

	return true;
}
//---------------------------------------------------------------------------
bool TBeam::BeamFromEllipse(TBeamInput *BeamPar)
{
	double sx=0,sy=0,r=0,th=0,x=0,y=0;
	double a=0,b=0,phi=0;
	TPhaseSpace C;

	phi=BeamPar->Ell.phi;
	b=BeamPar->Ell.by;
	a=BeamPar->Ell.ax;

	sx=a/BeamPar->Ell.h;
	sy=b/BeamPar->Ell.h;

	double xx=0,yy=0,Rc=0,Ran1=0,Ran2=0,Myx=0,Dyx=0;
	double Mx=0; //x0
	double My=0; //y0

	for (int i=0;i<Np;i++){
		do{
			TwoRandomGauss(Ran1,Ran2);
			xx=Mx+Ran1*sx;
			Myx=My+Rc*sy*(xx-Mx)/sx; //ellitic distribution
			Dyx=sqr(sy)*(1-sqr(Rc)); //dispersion
			yy=Myx+Ran2*sqrt(Dyx);

		} while (sqr(xx/a)+sqr(yy/b)>1);

		x=xx*cos(phi)-yy*sin(phi);
		y=xx*sin(phi)+yy*cos(phi);

		C=CartesianToCylinrical(x,y,0,0);

		r=C.x;
		th=C.y;

		Particle[i].r=r/lmb;
		Particle[i].Br=0;
		Particle[i].Th=th;
		Particle[i].Bth=0;
		Particle[i].Bz=BzFromOther(Particle[i].beta,Particle[i].Br,0);//double-check!!!
	}

	return true;
}
//---------------------------------------------------------------------------
TPhaseSpace *TBeam::MakeTwissDistribution(TTwiss T)
{
	TPhaseSpace *X=NULL;
	TEllipse E;

	X=new TPhaseSpace[Np];

	T.gamma=(1+sqr(T.alpha))/T.beta;
	E=GetEllipse(T);

	double xx=0,yy=0,Rc=0,Ran1=0,Ran2=0,Myx=0,Dyx=0;
	double Mx=0; //x0
	double My=0; //x'0

	for (int i=0;i<Np;i++){
		TwoRandomGauss(Ran1,Ran2);
		xx=Mx+Ran1*E.Rx;
		Myx=My+Rc*E.Ry*(xx-Mx)/E.Rx; //elliptic distribution
		Dyx=sqr(E.Ry)*(1-sqr(Rc)); //dispersion
		yy=Myx+Ran2*sqrt(Dyx);
		X[i].x=xx*cos(E.phi)-yy*sin(E.phi);
	  //	Particle[i].x=x/lmb;
		X[i].px=xx*sin(E.phi)+yy*cos(E.phi);     //x'=bx/bz; Emittance is defined in space x-x'
	   //	Particle[i].Bx=px*Particle[i].betta;  //bx=x'*bz
	}

   	return X;
}
//---------------------------------------------------------------------------
void TBeam::SetParameters(double *X,TBeamParameter Par)
{
	switch (Par) {
		case (R_PAR):{
			for (int i=0;i<Np;i++)
				Particle[i].r=X[i];
			break;
		}
		case (BR_PAR):{
            for (int i=0;i<Np;i++)
				Particle[i].Br=X[i];
            break;
        }
		case (TH_PAR):{
            for (int i=0;i<Np;i++)
                Particle[i].Th=X[i];
            break;
        }
        case (BTH_PAR):{
            for (int i=0;i<Np;i++)
                Particle[i].Bth=X[i];
            break;
        }
		case (PHI_PAR):{
            for (int i=0;i<Np;i++)
                Particle[i].phi=X[i];
            break;
        }
		case (BETA_PAR):{
			for (int i=0;i<Np;i++)
				Particle[i].beta=MeVToVelocity(X[i]);
			break;
		}
		case (BZ_PAR):{
			for (int i=0;i<Np;i++)
				Particle[i].Bz=X[i];
			break;
		}
	}
}
//---------------------------------------------------------------------------
void TBeam::SetCurrent(double I)
{
	Ib=I;
}
//---------------------------------------------------------------------------
void TBeam::SetInputCurrent(double I)
{
	I0=I;
}
//---------------------------------------------------------------------------
double *TBeam::MakeEquiprobableDistribution(double Xav, double dX)
{
	double Ran=0;
	double *Xi;
	Xi= new double[Np];
	double a=Xav-dX;
	double b=Xav+dX;

	for (int i=0;i<Np;i++){
		Ran=((double)rand() / ((double)RAND_MAX + 1));
		Xi[i]=a+(b-a)*Ran;
	}
	return Xi;
}
//---------------------------------------------------------------------------
void TBeam::MakeEquiprobableDistribution(double Xav, double dX,TBeamParameter Par)
{
	double *Xi;
	Xi=MakeEquiprobableDistribution(Xav,dX);
    SetParameters(Xi,Par);
	delete[] Xi;
}
//---------------------------------------------------------------------------
double *TBeam::MakeGaussDistribution(TGauss G)
{
	return MakeGaussDistribution(G.mean,G.sigma,G.limit);
}
//---------------------------------------------------------------------------
double *TBeam::MakeRayleighDistribution(TGauss G)
{
	return MakeRayleighDistribution(G.mean,G.sigma,G.limit);
}
//---------------------------------------------------------------------------
double *TBeam::MakeGaussDistribution(double Xav,double sX,double Xlim)
{
	double *Xi;
	double xx=Xlim+1;

	if (Xlim<=0)
		Xi=MakeGaussDistribution(Xav,sX);
	else if (sqr(Xlim)<12*sqr(sX))
		Xi=MakeEquiprobableDistribution(Xav,Xlim);
	else {
		Xi=new double[Np];
		for (int i=0;i<Np;i++){
			do {
				xx=RandomGauss()*sX/h;
			} while (mod(xx)>Xlim);
			Xi[i]=Xav+xx;
		}
	}

	return Xi;
}  //---------------------------------------------------------------------------
double *TBeam::MakeRayleighDistribution(double Xav,double sX,double Xlim)
{
	double *Xi;
	double xx=Xlim+1;

	if (Xlim<=0)
		Xi=MakeRayleighDistribution(Xav,sX);
  /*	else if (sqr(Xlim)<12*sqr(sX))
		Xi=MakeEquiprobableDistribution(Xav,Xlim);   */
	else {
		Xi=new double[Np];
		for (int i=0;i<Np;i++){
			do {
				xx=RandomRayleigh()*sX;
			} while (mod(xx)>Xlim);
			Xi[i]=Xav+xx;
		}
	}

	return Xi;
}
//---------------------------------------------------------------------------
double *TBeam::MakeGaussDistribution(double Xav,double sX)
{
	double xx=0;
	double *Xi;
	Xi= new double[Np];

	for (int i=0;i<Np;i++){
		xx=RandomGauss();
		Xi[i]=Xav+xx*sX/h;
	}
	return Xi;
}
//---------------------------------------------------------------------------
double *TBeam::MakeRayleighDistribution(double Xav,double sX)
{
	double Ran1=0,Ran2=0,xx=0;
	double *Xi;
	Xi= new double[Np];

	for (int i=0;i<Np;i++){
		xx=RandomRayleigh();
		Xi[i]=Xav+xx*sX;
	}
	return Xi;
}
//---------------------------------------------------------------------------
void TBeam::MakeGaussDistribution(double Xav,double sX,TBeamParameter Par,double Xlim)
{
	double *Xi;
	Xi=MakeGaussDistribution(Xav,sX,Xlim);
	SetParameters(Xi,Par);
	delete[] Xi;
}
//---------------------------------------------------------------------------
void TBeam::MakeGaussDistribution(TGauss G,TBeamParameter Par)
{
	return MakeGaussDistribution(G.mean,G.sigma,Par,G.limit);
}
//---------------------------------------------------------------------------
void TBeam::CountLiving()
{
	Nliv=0;
	for (int i=0;i<Np;i++){
		if (Particle[i].lost==LIVE)
			Nliv++;
	}
}
//---------------------------------------------------------------------------
TEllipse TBeam::FindEmittanceAngle(TBeamParameter P)
{
	TBeam *Beam1;
	Beam1=new TBeam(Np);
	Beam1->SetBarsNumber(Nbars);
	Beam1->SetKernel(Kernel);

	double Ang0=-pi/2, Ang1=pi/2, dAngle,Angle;
    double Angle1,Angle2,Res;
    double Sx=0,Sbx=0;
    double Sy=0,Sby=0;
	double Err=1e-5;
	int Nmax=30;

   //	TGauss Gx0,Gy0;
	double *X0,*Bx0;
	TBeamParameter P1,P2;

   //	FILE *logFile;
   //	logFile=fopen("Ang_new.log","w");

	P1=ComplementaryParameter(P);
	switch (P) {
		case R_PAR:{P2=BR_PAR;break;}
		case X_PAR:{P2=BX_PAR;break;}
		case Y_PAR:{P2=BY_PAR;break;}
		default: {P2=NO_PAR;};
	}

	X0=GetLivingParameter(P); //r/lmb
	Bx0=GetLivingParameter(P1); //rad

	for (int k=0;k<Nliv;k++){
		X0[k]*=lmb;
	   //	fprintf(logFile,"%f %f \n",X0[k],Bx0[k]);
	}

	dAngle=(Ang1-Ang0)/2;
	Angle=Ang0+dAngle;

    int i=0;
	while (dAngle>Err && i<Nmax){
        dAngle/=2;
        Angle1=Angle+dAngle;
		Angle2=Angle-dAngle;
		for (int k=0;k<Np;k++){
			Beam1->Particle[k].lost=Particle[k].lost;
			Beam1->Particle[k].beta=Particle[k].beta;
			Beam1->Particle[k].Th=Particle[k].Th;
			if (Particle[k].lost==LIVE){
				Beam1->Particle[k].r=X0[k]*cos(Angle1)+Bx0[k]*sin(Angle1);
				Beam1->Particle[k].Br=Bx0[k]*cos(Angle1)-X0[k]*sin(Angle1);
			  //	fprintf(logFile,"%f %f %f %f \n",X0[k],Bx0[k],Beam1->Particle[k].r,Beam1->Particle[k].Br);
            }
		}
		Sx=Beam1->GetDeviation(R_PAR);
		Sbx=Beam1->GetDeviation(BR_PAR);

        for (int k=0;k<Np;k++){
            if (Particle[k].lost==LIVE){
				Beam1->Particle[k].r=X0[k]*cos(Angle2)+Bx0[k]*sin(Angle2);
				Beam1->Particle[k].Br=Bx0[k]*cos(Angle2)-X0[k]*sin(Angle2);
            }
        }

		Sy=Beam1->GetDeviation(R_PAR);

		if (mod(Sx-Sy)<Err)
			break;
		else if (Sx>Sy)
            Angle=Angle2;
        else if (Sy>Sx)
            Angle=Angle1;
	   /* else if (Sx==Sy)
			break;  */
        i++;
	}

	delete Beam1;
	delete[] X0;delete[] Bx0;

   //	fclose(logFile);

	TEllipse E;
	E.ax=Sx;
	E.by=Sbx;
	E.phi=Angle;

	return E;
}
//---------------------------------------------------------------------------
TEllipse TBeam::GetEllipseDirect(TBeamParameter P)
{
	TEllipse E,E1;

	TGauss Gx,Gy;

	if (GetLivingNumber()>1) {
		TBeamParameter P1=ComplementaryParameter(P);
		Gx=GetStatistics(P); //r/lmb
		Gy=GetStatistics(P1); //rad
	}

	E.x0=Gx.mean; //x
	E.y0=Gy.mean;// x'
	E.Rx=Gx.sigma;
	E.Ry=Gy.sigma;

//	E.phi=FindEmittanceAngle(E.ax,E.by);

	E1=FindEmittanceAngle(P);
	E.ax=E1.ax;
	E.by=E1.by;

	E.phi=E1.phi;

	return E;
}
//---------------------------------------------------------------------------
TTwiss TBeam::GetTwissDirect(TBeamParameter P)
{
	TEllipse E;
	TTwiss T;

	E=GetEllipseDirect(P);
	T.epsilon=E.ax*E.by;
	T.beta=sqr(E.Rx)/T.epsilon;
	T.gamma=sqr(E.Ry)/T.epsilon;
	if (T.gamma*T.beta>1)
		T.alpha=sqrt(T.gamma*T.beta-1);
	else
		T.alpha=0;

	return T;
}
//---------------------------------------------------------------------------
TEllipse TBeam::GetEllipse(TTwiss T)
{
	TEllipse E;
	double H=0;

	T.gamma=(1+sqr(T.alpha))/T.beta;
	H=(T.beta+T.gamma)/2;

	//phi=0.5*arctg(2*alpha*betta/(1+sqr(alpha)-sqr(betta))); - old
	E.phi=0.5*arctg(-2*T.alpha/(T.beta-T.gamma));
	E.by=sqrt(T.epsilon/2)*(sqrt(H+1)+sqrt(H-1));
	E.ax=sqrt(T.epsilon/2)*(sqrt(H+1)-sqrt(H-1));

	E.Rx=E.ax;///h; //sigma RMS
	E.Ry=E.by;///h; //sigma RMS

	return E;
}
//---------------------------------------------------------------------------
TEllipse TBeam::GetEllipse(TBeamParameter P)
{
	TEllipse E;
	TTwiss T;
	TGauss Gx,Gy;

	Gx.mean=0;
	Gy.mean=0;

	T=GetTwiss(P);
	E=GetEllipse(T);

	if (GetLivingNumber()>1) {
		TBeamParameter P1=ComplementaryParameter(P);
		Gx=GetStatistics(P); //r/lmb
		Gy=GetStatistics(P1); //rad
	}

	E.x0=Gx.mean;
	E.y0=Gy.mean;

	return E;
}
//---------------------------------------------------------------------------
TTwiss TBeam::GetTwiss(TBeamParameter P, bool Norm)
{
	TTwiss T;
	double *X=NULL,*Y=NULL, *Z=NULL;
	TGauss Gx,Gy,Gz;
	bool Square=true;

	T.epsilon=0;
	T.alpha=0;
	T.beta=0;

	//FILE *logFile;
	//logFile=fopen("TwissRead.log","w");
	TBeamParameter P1=ComplementaryParameter(P);

	if (GetLivingNumber()>1) {
		X=GetLivingParameter(P); //r/lmb
		Y=GetLivingParameter(P1); //rad
		if (P==R_PAR)
			Z=GetLivingParameter(ATH_PAR); //rad
	}

	double Sx=0,Spx=0,Sxpx=0,Spy=0,Sxpy=0;
	for (int i = 0; i < Nliv; i++) {
		//fprintf(logFile,"%f %f\n",X[i]*lmb,Y[i]);
		Sx+=sqr(X[i]);
		Spx+=sqr(Y[i]);
		Sxpx+=X[i]*Y[i];
		if (P==R_PAR) {
			Spy+=sqr(Z[i]);
			Sxpy+=X[i]*Z[i];
        }
	}

	if (GetLivingNumber()>1) {
		Gx=GetStatistics(P); //r/lmb
		Gy=GetStatistics(P1); // rad
		if (P==R_PAR)
			Gz=GetStatistics(ATH_PAR);
	}

	if (P==R_PAR) {
		T.epsilon=sqrt(Sx*(Spx+Spy)-sqr(Sxpx)-sqr(Sxpy))/(2*Nliv);
		if (T.epsilon==0) {
        	T.epsilon=1e-12;
		}

		T.beta=sqr(Gx.sigma)/T.epsilon;
		T.gamma=(sqr(Gy.sigma)+sqr(Gz.sigma))/T.epsilon;

		if (T.gamma*T.beta-1>0)
			T.alpha=sqrt(T.gamma*T.beta-1);
		else
			T.alpha=-((Sxpx+Sxpy)/Nliv)/T.epsilon;
		/*T.beta=sqr(Gx.sigma)/T.epsilon;

		T.gamma=(sqr(T.alpha)+1)/T.beta;     */
	}else{
		T.epsilon=sqrt(Sx*Spx-sqr(Sxpx))/Nliv;
		if (T.epsilon==0) {
        	T.epsilon=1e-12;
		}
		T.beta=sqr(Gx.sigma)/T.epsilon;
		T.gamma=sqr(Gy.sigma)/T.epsilon;
		if (T.gamma*T.beta-1>0) {
			T.alpha=sqrt(T.gamma*T.beta-1);
		} else
             T.alpha=-(Sxpx/Nliv)/T.epsilon;

	}

	if (Norm) {
		double W=GetAverageEnergy();
		double beta_gamma=MeVToVelocity(W)*MeVToGamma(W);
		T.epsilon=beta_gamma*T.epsilon;
		T.beta=T.beta/beta_gamma;
	}
	DeleteArray(X);
	DeleteArray(Y);
	DeleteArray(Z);

	//fclose(logFile);

	return T;
}
//---------------------------------------------------------------------------
double TBeam::GetParameter(int i,TBeamParameter P)
{
	double x=0;
	TPhaseSpace C,R;

    if (IsRectangular(P)) {
		C.x=Particle[i].r;
		C.y=Particle[i].Th;
		C.px=Particle[i].Br/Particle[i].beta;
		C.py=Particle[i].Bth/Particle[i].beta;
		R=CylinricalToCartesian(C);
	}

	switch (P) {
		case (R_PAR):{
			x=Particle[i].r*lmb;
			break;
		}
		case (TH_PAR):{
			x=Particle[i].Th;
			break;
		}
		case (X_PAR):{
			x=R.x*lmb;
			break;
		}
		case (Y_PAR):{
			x=R.y*lmb;
			break;
		}
		case (BR_PAR):{
			x=Particle[i].Br;
			break;
		}
		case (BTH_PAR):{
			x=Particle[i].Bth;
			break;
		}
		case (BX_PAR):{
			x=R.px*Particle[i].beta;
			break;
		}
		case (BY_PAR):{
			x=R.py*Particle[i].beta;
			break;
		}
		case (AR_PAR):{
			x=arctg(Particle[i].Br/Particle[i].beta);
			break;
		}
		case (ATH_PAR):{
			x=arctg(Particle[i].Bth/Particle[i].beta);
			break;
		}
		case (AX_PAR):{
			x=arctg(R.px);
			break;
		}
		case (AY_PAR):{
			x=arctg(R.py);
			break;
		}
		case (PHI_PAR):{
			x=Particle[i].phi;
			break;
		}
        case (ZREL_PAR):{
			x=lmb*Particle[i].phi/(2*pi);
			break;
		}
		case (Z0_PAR):{
			x=Particle[i].z;
			break;
		}
		case (BETA_PAR):{
			x=Particle[i].beta;
			break;
		}
		case (W_PAR):{
			x=VelocityToMeV(Particle[i].beta);
			break;
			}
		case (LIVE_PAR):{
			x=Particle[i].lost==LIVE?1:0;
			break;
			}
		default:{
			x=-1;
			break;
		}
	}

	return x;
}
//---------------------------------------------------------------------------
TBeamParameter TBeam::ComplementaryParameter(TBeamParameter P)
{
	TBeamParameter P1;
	switch (P) {
		case R_PAR:{P1=AR_PAR;break;}
		case TH_PAR:{P1=BTH_PAR;break;}
		case X_PAR:{P1=AX_PAR;break;}
		case Y_PAR:{P1=AY_PAR;break;}
		default: {P1=P;};
	}

	return P1;
}
//---------------------------------------------------------------------------
bool TBeam::IsRectangular(TBeamParameter P)
{
	bool Rec=false;

	switch (P) {
		case X_PAR:{}
		case Y_PAR:{}
		case AX_PAR:{}
		case AY_PAR:{}
		case BX_PAR:{}
		case BY_PAR:{
			Rec=true;
			break;
		}
		default: Rec=false;
	}

	return Rec;
}
//---------------------------------------------------------------------------
double *TBeam::GetLivingParameter(TBeamParameter P)
{
	double *X=NULL;
	CountLiving();
	X=new double[Nliv];
	int j=0;

	for (int i=0;i<Np;i++){
		if (Particle[i].lost==LIVE) {
			X[j]=GetParameter(i,P);
			j++;
		}
	}

	return X;
}
//---------------------------------------------------------------------------
TGauss TBeam::GetStatistics(TBeamParameter P,bool FWHM)
{
	TGauss G;
	double *X=GetLivingParameter(P);

	G=GetStatistics(X,FWHM);

	//delete[] X;

	return G;
}
//---------------------------------------------------------------------------
TGauss TBeam::GetStatistics(double *X,bool FWHM)
{
	TGauss G;

	TSpectrum *Spectrum;
	Spectrum=GetSpectrum(X);

	G.mean=Spectrum->GetAverage();
	G.FWHM=FWHM?Spectrum->GetWidth():0;
	G.sigma=Spectrum->GetSquareDeviation();

	delete Spectrum;
	//delete[] X;

	return G;
}
//---------------------------------------------------------------------------
TSpectrum *TBeam::GetSpectrum(TBeamParameter P)
{
	double *X=GetLivingParameter(P);

	TSpectrum *Spectrum=GetSpectrum(X);

	//delete[] X;

	return Spectrum;
}
//---------------------------------------------------------------------------
TSpectrum *TBeam::GetSpectrum(double *X)
{
	TSpectrum *Spectrum;
	Spectrum=new TSpectrum;

	Spectrum->SetMesh(X,Nbars,Nliv);
	delete[] X;

	return Spectrum;
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeam::GetSpectrumBar(TBeamParameter P,bool Smooth)
{
	double *X=GetLivingParameter(P);

	TSpectrumBar *SpectrumBar=GetSpectrumBar(X,Smooth);

	//delete[] X;

	return SpectrumBar;
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeam::GetSpectrumBar(double *X,bool Smooth)
{
	TSpectrum *Spectrum=GetSpectrum(X);
	TSpectrumBar *S=Spectrum->GetSpectrum(Smooth);

	TSpectrumBar *SpectrumArray;
	SpectrumArray=new TSpectrumBar[Nbars];

	for (int i=0;i<Nbars;i++) {
		 SpectrumArray[i]=S[i];
	}

	delete Spectrum;
	//delete[] X;

	return SpectrumArray;
}
//---------------------------------------------------------------------------
//OBSOLETE
/*TSpectrumBar *TBeam::GetSpectrum(bool Smooth,double *X,double& Xav,double& dX,bool width)
{
	TSpectrumBar *S,*SpectrumArray;
	TSpectrum *Spectrum;
	Spectrum=new TSpectrum;

	Spectrum->SetMesh(X,Nbars,Nliv);
    //if (Xav!=NULL)
		Xav=Spectrum->GetAverage();
    if (width)
		dX=Spectrum->GetWidth();
    else
		dX=Spectrum->GetSquareDeviation();

	S=Spectrum->GetSpectrum(Smooth && dX!=0);
	SpectrumArray=new TSpectrumBar[Nbars];
	for (int i=0;i<Nbars;i++) {
		 SpectrumArray[i]=S[i];
	}

	delete[] X;
	delete Spectrum;

	return SpectrumArray;
}                 */
//---------------------------------------------------------------------------
TGauss TBeam::GetEnergyDistribution(TDeviation D)
{
	return GetStatistics(W_PAR,D==D_FWHM);
}
//---------------------------------------------------------------------------
TGauss TBeam::GetPhaseDistribution(TDeviation D)
{
	return GetStatistics(PHI_PAR,D==D_FWHM);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeam::GetEnergySpectrum(bool Smooth)
{
	return GetSpectrumBar(W_PAR,Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeam::GetPhaseSpectrum(bool Smooth)
{
	return GetSpectrumBar(PHI_PAR,Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeam::GetRSpectrum(bool Smooth)
{
	return GetSpectrumBar(R_PAR,Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeam::GetXSpectrum(bool Smooth)
{
	return GetSpectrumBar(X_PAR,Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeam::GetYSpectrum(bool Smooth)
{
	return GetSpectrumBar(Y_PAR,Smooth);
}
//---------------------------------------------------------------------------
double TBeam::GetPhaseLength(TDeviation D)
{
    return GetDeviation(PHI_PAR,D);
}
//---------------------------------------------------------------------------
double TBeam::GetMinPhase()
{
	return GetMinValue(PHI_PAR);
}
//---------------------------------------------------------------------------
double TBeam::GetMaxPhase()
{
	return GetMaxValue(PHI_PAR);
}
//---------------------------------------------------------------------------
double TBeam::GetMaxDivergence()
{
	double Xmin=mod(GetMinValue(AR_PAR));
	double Xmax=mod(GetMaxValue(AR_PAR));

	return Xmin>Xmax?Xmin:Xmax;
}
//---------------------------------------------------------------------------
double TBeam::GetAveragePhase()
{
	return GetMean(PHI_PAR);
}
//---------------------------------------------------------------------------
double TBeam::GetAverageEnergy()
{
	return GetMean(W_PAR);
}
//---------------------------------------------------------------------------
double TBeam::GetMaxEnergy()
{
	return GetMaxValue(W_PAR);
}
//---------------------------------------------------------------------------
double TBeam::GetCurrent()
{
	return Ib;
}
//---------------------------------------------------------------------------
double TBeam::GetInputCurrent()
{
	return I0;
}
//---------------------------------------------------------------------------
double TBeam::GetMean(TBeamParameter P)
{
	TGauss G;
	G=GetStatistics(P);

	return G.mean;
}
//---------------------------------------------------------------------------
double TBeam::GetDeviation(TBeamParameter P,TDeviation D)
{
	TGauss G;
	G=GetStatistics(P,D==D_FWHM);
	double sx=D==D_FWHM?G.FWHM:G.sigma;

	return sx;
}
//---------------------------------------------------------------------------
double TBeam::GetMinValue(TBeamParameter P)
{
	double x_min=1e32;
	double *X=GetLivingParameter(P);

	for (int i=0;i<Nliv;i++){
		if (X[i]<x_min)
			x_min=X[i];
	}
	delete[] X;

	return x_min;
}
//---------------------------------------------------------------------------
double TBeam::GetMaxValue(TBeamParameter P)
{
	double x_max=-1e32;
	double *X=GetLivingParameter(P);

	for (int i=0;i<Nliv;i++){
		if (X[i]>x_max)
			x_max=X[i];
	}
	delete[] X;

	return x_max;
}
//---------------------------------------------------------------------------
double TBeam::GetRadius(TBeamParameter P,TDeviation D)
{
	double Rb=GetDeviation(P,D);

	return Rb;
}
//---------------------------------------------------------------------------
double TBeam::GetDivergence(TBeamParameter P,TDeviation D)
{
	return GetDeviation(P,D);
}
//---------------------------------------------------------------------------
int TBeam::GetLivingNumber()
{
    CountLiving();
    return Nliv;
}
//---------------------------------------------------------------------------
double TBeam::SinSum(TIntParameters& Par,TIntegration *I)
{
    return BesselSum(Par,I,SIN);
}
//---------------------------------------------------------------------------
double TBeam::CosSum(TIntParameters& Par,TIntegration *I)
{
    return BesselSum(Par,I,COS);
}
//---------------------------------------------------------------------------
double TBeam::BesselSum(TIntParameters& Par,TIntegration *I,TTrig Trig)
{
    double S=0,N=0,S1=0;
    double phi=0,r=0,bw=0,c=0;
    double Res=0;

    for (int i=0;i<Np;i++){
        if (Particle[i].lost==LIVE){
            phi=Particle[i].phi+I[i].phi*Par.h;
			r=Particle[i].r+I[i].r*Par.h;
            bw=Par.bw;
            if (Trig==SIN)
                c=sin(phi);
            else if (Trig==COS)
                c=cos(phi);

            double arg=2*pi*sqrt(1-sqr(bw))*r/bw;
            double Bes=Ib0(arg)*c;
           /*   if (mod(Bes)>100)
                ShowMessage("this!");      */
            S+=Bes;
            N++;
        }
    }

    Res=N>0?S/N:0;

    return Res;
}
//---------------------------------------------------------------------------
double TBeam::iGetAverageEnergy(TIntParameters& Par,TIntegration *I)
{
    double gamma=1;
    bool err=false;
    int j=0;

    double G=0,Gav=0,dB=0;
   //   TSpectrumBar *Spectrum;
    

    //logFile=fopen("beam.log","w");

    for (int i=0;i<Np;i++){
        if (Particle[i].lost==LIVE){
			double betta=Particle[i].beta+I[i].betta*Par.h;
			double bx=Particle[i].Br+I[i].bx*Par.h;
            // b=sqrt(sqr(betta)+sqr(bx));
            double b=betta;
            if (mod(betta)>=1)
                Particle[i].lost=STEP_LOST;//BZ_LOST;
        /*  else if (mod(bx)>=1)
                Particle[i].lost=STEP_LOST;//BX_LOST;
            else if (mod(b)>=1)
                Particle[i].lost=STEP_LOST;    */
            else{
                G+=VelocityToEnergy(b);
                j++;
            }
          //    fprintf(logFile,"%f %f %f\n",Particle[i].betta,I[i].betta,Par.h);
        }
    }

   //   fclose(logFile);

   //   Spectrum=GetSpectrum(false,B,Bav,dB);
    //delete[] Spectrum;
    if (j>0)
        Gav=G/j;
    else
        Gav=0;

    gamma=Gav;

    return gamma;
}
//---------------------------------------------------------------------------
double TBeam::iGetBeamLength(TIntParameters& Par,TIntegration *I, int Nslices, bool SpectrumOutput)
{
    double L=1,Fmin=1e32,Fmax=-1e32;
    int j=0;

	/*double F=NULL;*/
	double phi=0,Fav=0,dF=0;;
	/*double R=NULL;*/
	double r=0;
	double FavPhase=0,dPhase=0;
    TSpectrumBar *SpectrumPhase;
    for (int i=0;i<Np;i++){
        if (Particle[i].lost==LIVE){
            phi=Particle[i].phi+I[i].phi*Par.h;
           /*   if (phi<=HellwegTypes::DegToRad(MinPhase) || phi>=HellwegTypes::DegToRad(MaxPhase) )
                Particle[i].lost=PHASE_LOST; */
        }
    }

    CountLiving();

  /*  F=new double[Nliv];
    R=new double[Nliv];   */
    for (int i=0;i<Np;i++){
        if (Particle[i].lost==LIVE){
            phi=Particle[i].phi+I[i].phi*Par.h;
			r=Particle[i].r+I[i].r*Par.h;
		 //   F[j]=phi;
		  //	R[j]=r;
            j++; 
//            double phi=Particle[i].phi+I[i].phi*Par.h;
            if (phi>Fmax)
                Fmax=phi;
            if (phi<Fmin)
                Fmin=phi;
        }
    }


   /*   Spectrum=GetSpectrum(false,F,Fav,dF,true);
    delete[] Spectrum;    */
    dF=mod(Fmax-Fmin);
    L=dF*lmb/(2*pi);
	
   /*	SpectrumPhase=GetPhaseSpectrum(false,R,F,FavPhase,dPhase,Nslices,false);
    delete[] SpectrumPhase;*/

	return L;
}
//---------------------------------------------------------------------------
double TBeam::iGetAveragePhase(TIntParameters& Par,TIntegration *I)
{
    double F=0,Fav=0,dF=0;
   //   TSpectrumBar *Spectrum;
    /*CountLiving();
    F=new double[Nliv];*/
    int j=0;

    for (int i=0;i<Np;i++){
        if (Particle[i].lost==LIVE){
            double phi=Particle[i].phi+I[i].phi*Par.h;
           /*   if (phi<=HellwegTypes::DegToRad(MinPhase) || phi>=HellwegTypes::DegToRad(MaxPhase) )
                Particle[i].lost=PHASE_LOST;
            else{      */
                F+=phi;
                j++;
            //}
        }
    }

   /*   Spectrum=GetSpectrum(false,F,Fav,dF);
    delete[] Spectrum;*/
    if (j>0)
        Fav=F/j;
    else
        Fav=0;

    return Fav;
}
//---------------------------------------------------------------------------
double TBeam::iGetBeamRadius(TIntParameters& Par,TIntegration *I,bool SpectrumOutput)
{
    double X=1;
    int j=0;

    double *R,Rav=0,dR=0;
    TSpectrumBar *Spectrum;
    CountLiving();
    R=new double[Nliv];

    for (int i=0;i<Np;i++){
        if (Particle[i].lost==LIVE){
			double r=Particle[i].r+I[i].r*Par.h;
            R[j]=r;
            j++;
        }
	}

	TGauss Gr=GetStatistics(R);
	dR=Gr.sigma;

   /* Spectrum=GetSpectrum(false,R,Rav,dR,false);
	delete[] Spectrum;    */

    X=dR*lmb;

    return X;
}
//---------------------------------------------------------------------------
void TBeam::Integrate(TIntParameters& Par,TIntegration **I,int Si)
{
    double bz=1,Sr=0,gamma=1,C=0;
    double k_phi=0,k_Az=0,k_Ar=0,k_Hth=0,k_bz=0,k_bx=0,k_r=0,k_th=0,k_A=0,A=0,dA=0,th_dot=0;
    int Sj=0;
    double r=0,r0=0,phi=0,bx=0,bth=0;
    double s=-1;
    double rev=1;
	
		if (Reverse)
        rev=-1;

    Sj=(Si+1<Ncoef)?Si+1:0;

    CountLiving();

    Par.B*=Ib;

    if (Par.drift)
        I[Sj][0].A=0;//I[Si][0].A;
    else{
        A=Par.A+I[Si][0].A*Par.h;
        I[Sj][0].A=A*(Par.dL-rev*Par.w)-rev*2*Par.B*Par.SumCos; 
        dA=I[Sj][0].A;
    }

  /*    if (!Par.drift){
        A=Par.A+I[Si][0].A*Par.h;
        dA=I[Si][0].A;
    }      */
 
    //logFile=fopen("beam.log","w");

    for (int i=0;i<Np;i++){
        if (Particle[i].lost==LIVE){
			bz=Particle[i].beta+I[Si][i].betta*Par.h;
			bx=Particle[i].Br+I[Si][i].bx*Par.h;
            //gamma=VelocityToEnergy(sqrt(sqr(bz)+sqr(bx)));
            gamma=VelocityToEnergy(bz);
			r=Particle[i].r+I[Si][i].r*Par.h;
			r0=Particle[i].r0;
           //   bth=I[Si][i].th*r*lmb;
            phi=Particle[i].phi+I[Si][i].phi*Par.h;
            Sr=2*pi*sqrt(1-sqr(Par.bw))/Par.bw;

			if (!Par.drift)
                k_phi=2*pi*(1/Par.bw-1/bz)+2*Par.B*Par.SumSin/A;
            else
                k_phi=2*pi*(1/Par.bw-1/bz);

		  /*  if (i==100){
                FILE *F;
                F=fopen("beam.log","a");
                fprintf(F,"%f %f\n",bz,k_phi);
                fclose(F);
            }      */

			k_Az=A*Ib0(r*Sr)*cos(phi)+Par.Aqz[i];   //k_Az = Az
		 //	if (Par.bw!=1) {
				k_Ar=-(1/Sr)*Ib1(r*Sr)*(dA*cos(phi)-(2*Par.B*Par.SumSin+2*pi*A/Par.bw)*sin(phi))+Par.Aqr[i]; //k_Ar = Ar
		 /*	} else {
				k_Ar=-(r/2)*(dA*cos(phi)-(2*Par.B*Par.SumSin+2*pi*A/Par.bw)*sin(phi))+Par.Aqr[i];
			}         */


			//k_Ar=-Par.bw*Ib1(Sr)*(dA*cos(phi)-(2*Par.B*Par.SumSin+2*pi*A/Par.bw)*sin(phi))/(2*pi*sqrt(1-sqr(Par.bw)))+Par.Aqr;
		//	if (Par.bw!=1) {
				k_Hth=(Par.bw*A*Ib1(r*Sr)*sin(phi))/sqrt(1-sqr(Par.bw));      //k_Hth = Hth
		 /*	} else {
				k_Hth=(pi*A*r*sin(phi));
			}        */
			k_bz=((1-sqr(bz))*k_Az+bx*(k_Hth-bz*k_Ar)-bth*Par.Br_ext)/(gamma*bz); //k_bz = dbz/dz
           //   k_th=-Par.Bz_ext/(2*gamma*bz);      //k_th = dbth/dz =
            if (r0==0)
                C=Par.Cmag;
            else
                C=Par.Cmag*sqr(r0/r);
            k_th=(C-Par.Bz_ext)/(2*gamma*bz);
            th_dot=k_th*bz;
            //bth=th_dot*r;
            //bth=k_th*r*lmb;
    //      k_bx=(k_Ar-bz*k_Hth-bx*(bz*k_Az+bx*k_Ar))/(gamma*bz)+s*((bth*Par.Bz_ext/(bz*gamma))+(r*sqr(Par.Bz_ext/(2*gamma*bz))/bz));       //previous
            k_bx=(k_Ar-bz*k_Hth-bx*(bz*k_Az+bx*k_Ar))/(gamma*bz)+th_dot*r*Par.Bz_ext/(gamma*bz)+r*sqr(th_dot)/bz;
            k_r=PulseToAngle(bx,bz);
           /*   if (mod(k_r)>100)
                k_r=1;   */

            I[Sj][i].phi=k_phi;
            I[Sj][i].Ar=k_Ar;
            I[Sj][i].Az=k_Az;
            I[Sj][i].Hth=k_Hth;
            I[Sj][i].betta=k_bz;
            I[Sj][i].bx=k_bx;
            I[Sj][i].th=k_th;
            I[Sj][i].r=k_r;
            I[Sj][i].bth=k_th;

        /*  if (i==200){
                fprintf(logFile,"bz=%f\n",bz);
                fprintf(logFile,"k_Az=%f\n",k_Az);
                fprintf(logFile,"bx=%f\n",bx);
                fprintf(logFile,"k_Hth=%f\n",k_Hth);
                fprintf(logFile,"k_Ar=%f\n",k_Ar);
                fprintf(logFile,"bth=%f\n",bth);
                fprintf(logFile,"Br_ext=%f\n",Par.Br_ext);
                fprintf(logFile,"gamma=%f\n",Par.gamma);
            }      */
        }
    }

   //fclose(logFile);
}
//---------------------------------------------------------------------------
void TBeam::Next(TBeam *nBeam,TIntParameters& Par,TIntegration **I)
{
    TParticle *nParticle;
    nParticle=nBeam->Particle;
    int Nbx=0,Nb=0;
    float b=0;
  //    logFile=fopen("beam.log","w");

    for (int i=0;i<Np;i++){
        nParticle[i].lost=Particle[i].lost;
        if (Particle[i].lost==LIVE){
			nParticle[i].r=Particle[i].r+(I[0][i].r+I[1][i].r+2*I[2][i].r+2*I[3][i].r)*Par.h/6;
            nParticle[i].Th=Particle[i].Th+(I[0][i].th+I[1][i].th+2*I[2][i].th+2*I[3][i].th)*Par.h/6;

			nParticle[i].Br=0;
			nParticle[i].Br=Particle[i].Br+(I[0][i].bx+I[1][i].bx+2*I[2][i].bx+2*I[3][i].bx)*Par.h/6;
            nParticle[i].Bth=Particle[i].Bth+(I[0][i].bth+I[1][i].bth+2*I[2][i].bth+2*I[3][i].bth)*Par.h/6;

			if (nParticle[i].Br>=1 || nParticle[i].Br<=-1){
                nParticle[i].lost=BX_LOST;
      //            fprintf(logFile,"%f\n",nParticle[i].Bx);
               //   Nbx++;
            }
            nParticle[i].phi=0;
            nParticle[i].phi=Particle[i].phi+(I[0][i].phi+I[1][i].phi+2*I[2][i].phi+2*I[3][i].phi)*Par.h/6;
        /*  if (nParticle[i].phi>=HellwegTypes::DegToRad(MaxPhase) || nParticle[i].phi<=HellwegTypes::DegToRad(MinPhase)){
                nParticle[i].lost=PHASE_LOST;
            }    */
			nParticle[i].beta=0;
			nParticle[i].beta=Particle[i].beta+(I[0][i].betta+I[1][i].betta+2*I[2][i].betta+2*I[3][i].betta)*Par.h/6;
			if (nParticle[i].beta>=1 || nParticle[i].beta<=-1){
                nParticle[i].lost=BZ_LOST;
              //    Nb++;
            }
           //   fprintf(logFile,"%i %f %f %f %f %f\n",i,I[1][i].betta,I[2][i].betta,I[3][i].betta,I[0][i].betta,nParticle[i].betta);

			b=sqrt(sqr(nParticle[i].beta)+sqr(nParticle[i].Br));

            for (int j=0;j<4;j++){
                I[j][i].r=0;
                I[j][i].th=0;
                I[j][i].bx=0;
                I[j][i].bth=0;
                I[j][i].phi=0;
                I[j][i].betta=0;
                I[j][i].Ar=0;
                I[j][i].A=0;
                I[j][i].Az=0;
                I[j][i].Hth=0;
            }
           /*   if (b>1)
                nParticle[i].lost=B_LOST;   */

           //   nParticle[i].Bth=(nParticle[i].Th-Particle[i].Th)*nParticle[i].x*lmb;
        }
    }
    CountLiving();
    nBeam->Ib=I0*Nliv/Np;
    //fclose(logFile);
}
//---------------------------------------------------------------------------
void TBeam::Next(TBeam *nBeam)
{
    TParticle *nParticle;
    nParticle=nBeam->Particle;
    int Nbx=0,Nb=0;
    float b=0;

    for (int i=0;i<Np;i++){
        nParticle[i].lost=Particle[i].lost;
		nParticle[i].r=Particle[i].r;
        nParticle[i].Th=Particle[i].Th;
		nParticle[i].Br=Particle[i].Br;
        nParticle[i].phi=Particle[i].phi;
        nParticle[i].beta=Particle[i].beta;
        nParticle[i].Bth=Particle[i].Bth;
    }
    nBeam->Ib=Ib;
}
//---------------------------------------------------------------------------

#pragma package(smart_init)
