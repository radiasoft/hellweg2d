//---------------------------------------------------------------------------


#pragma hdrstop

#include "BeamSolver.h"
// #include "Types.h"

//---------------------------------------------------------------------------
__fastcall TBeamSolver::TBeamSolver(AnsiString UserIniPath)
{
    this->UserIniPath = UserIniPath;
    Initialize();
}
//---------------------------------------------------------------------------
__fastcall TBeamSolver::TBeamSolver()
{
    Initialize();
}
//---------------------------------------------------------------------------
__fastcall TBeamSolver::~TBeamSolver()
{
	delete[] StructPar.Cells;
	for (int i = 0; i < StructPar.NSections; i++)
		delete[] StructPar.Sections;

    delete[] Structure;

    for (int i=0;i<Npoints;i++)
        delete Beam[i];
    delete[] Beam;

    delete[] Par;
    for (int i=0;i<Ncoef;i++)
        delete[] K[i];
    delete[] K;

    #ifndef RSLINAC
    if (SmartProgress!=NULL)
        delete SmartProgress;
    #endif

    delete InputStrings;
    delete ParsedStrings;

	//fclose(logFile);
}
//---------------------------------------------------------------------------
void TBeamSolver::Initialize()
{
    MaxCells=500;
    Nmesh=20;
   	Kernel=0;
    SplineType=LSPLINE;
    Nstat=100;
    Ngraph=500;
	//Nbars=100; //default
    Nav=10;
    Smooth=0.95;
	Npoints=1;
	Ndump=0;

	//Np=1;
   //	NpEnergy=0;
	Np_beam=1;
	BeamPar.RBeamType=NOBEAM;
	BeamPar.ZBeamType=NOBEAM;
	BeamPar.NParticles=1;
   //	BeamPar.Magnetized=false;

	StructPar.NSections=0;
	StructPar.Sections=NULL;
	StructPar.NElements=0;
	StructPar.ElementsLimit=-1;

	LoadIniConstants();

	DataReady=false;
	Stop=false;

	K=new TIntegration*[Ncoef];
	for (int i=0;i<Ncoef;i++)
		K[i]=new TIntegration[BeamPar.NParticles];

	Par=new TIntParameters[Ncoef];

	StructPar.Cells = new TCell[1];

    Beam=new TBeam*[Npoints];
    for (int i=0;i<Npoints;i++) {
        Beam[i]=new TBeam(1);
    }

    Structure=new TStructure[Npoints];

    InputStrings=new TStringList;
	ParsedStrings=new TStringList;

	BeamExport=NULL;

    #ifndef RSLINAC
    SmartProgress=NULL;
    #endif
}
//---------------------------------------------------------------------------
void TBeamSolver::ResetStructure()
{
	StructPar.NSections=0;
	if (StructPar.Sections!=NULL) {
		delete[] StructPar.Sections;
		StructPar.Sections=NULL;
	}
	StructPar.NElements=0;
	if (StructPar.Cells!=NULL) {
		delete[] StructPar.Cells;
		StructPar.Cells=NULL;
	}
}
//---------------------------------------------------------------------------
void TBeamSolver::ResetDump(int Ns)
{
	if (BeamExport!=NULL){
		delete[] BeamExport;
		BeamExport=NULL;
	}

	Ndump=Ns;

	if (Ns>0) {
		BeamExport=new TDump[Ndump];
	}
}
//---------------------------------------------------------------------------
void TBeamSolver::ShowError(AnsiString &S)
{
        #ifndef RSLINAC
	ShowMessage(S);
        #endif
	ParsedStrings->Add(S);
}
//---------------------------------------------------------------------------
void TBeamSolver::SaveToFile(AnsiString& Fname)
{
    FILE *F;
    F=fopen(Fname.c_str(),"wb");

    fwrite(&Npoints,sizeof(int),1,F);
	fwrite(&BeamPar.NParticles,sizeof(int),1,F);
	//fwrite(&Nbars,sizeof(int),1,F);

	fwrite(Structure,sizeof(TStructure),Npoints,F);

    for(int i=0;i<Npoints;i++){
        fwrite(&(Beam[i]->lmb),sizeof(double),1,F);
		fwrite(&(Beam[i]->h),sizeof(double),1,F);
		double Ib=Beam[i]->GetCurrent();
		double I0=Beam[i]->GetInputCurrent();
		fwrite(&(Ib),sizeof(double),1,F);
		fwrite(&(I0),sizeof(double),1,F);
		fwrite(Beam[i]->Particle,sizeof(TParticle),BeamPar.NParticles,F);
    }

	fclose(F);
}
//---------------------------------------------------------------------------
bool TBeamSolver::LoadFromFile(AnsiString& Fname)
{
    FILE *F;
    F=fopen(Fname.c_str(),"rb");
    bool Success;

    delete[] Structure;

    for (int i=0;i<Np_beam;i++)
        delete Beam[i];
    delete[] Beam;

    Beam=new TBeam*[Npoints];
    for (int i=0;i<Npoints;i++)
		Beam[i]=new TBeam(BeamPar.NParticles);
	Np_beam=Npoints;

    try{
        fread(&Npoints,sizeof(int),1,F);
		fread(&BeamPar.NParticles,sizeof(int),1,F);
	  //  fread(&Nbars,sizeof(int),1,F);

        Structure=new TStructure[Npoints];
        Beam=new TBeam*[Npoints];
        for (int i=0;i<Npoints;i++)
			Beam[i]=new TBeam(BeamPar.NParticles);

        fread(Structure,sizeof(TStructure),Npoints,F);
      //    fread(Beam,sizeof(TBeam),Npoints,F);


        for (int i=0;i<Npoints;i++){
            fread(&(Beam[i]->lmb),sizeof(double),1,F);
			fread(&(Beam[i]->h),sizeof(double),1,F);
			double Ib=0, I0=0;
			fread(&(Ib),sizeof(double),1,F);
			fread(&(I0),sizeof(double),1,F);
			Beam[i]->SetCurrent(Ib);
            Beam[i]->SetInputCurrent(I0);
			fread(Beam[i]->Particle,sizeof(TParticle),BeamPar.NParticles,F);
        }
        Success=true;
    } catch (...){
        Success=false;
    }

    fclose(F);
    return Success;
}
//---------------------------------------------------------------------------
#ifndef RSLINAC
void TBeamSolver::AssignSolverPanel(TObject *SolverPanel)
{
    SmartProgress=new TSmartProgress(static_cast <TWinControl *>(SolverPanel));
}
#endif
//---------------------------------------------------------------------------
void TBeamSolver::LoadIniConstants()
{
    TIniFile *UserIni;
    int t;
    double stat;

    UserIni=new TIniFile(UserIniPath);
	MaxCells=UserIni->ReadInteger("OTHER","Maximum Cells",MaxCells);
	Nmesh=UserIni->ReadInteger("NUMERIC","Number of Mesh Points",Nmesh);
	Kernel=UserIni->ReadFloat("Beam","Percent Of Particles in Kernel",Kernel);
    if (Kernel>0)
    	Kernel/=100;
    	else
    	Kernel=0.9;


    t=UserIni->ReadInteger("NUMERIC","Spline Interpolation",t);
    switch (t) {
        case (0):{
            SplineType=LSPLINE;
            break;
        }
        case (1):{
            SplineType=CSPLINE;
            break;
        }
        case (2):{
            SplineType=SSPLINE;
            break;
        }
    }

    stat=UserIni->ReadFloat("NUMERIC","Statistics Error",stat);
    if (stat<1e-6)
        stat=1e-6;
    if (stat>25)
        stat=25;
	int Nstat=round(100.0/stat);
	AngErr=UserIni->ReadFloat("NUMERIC","Angle Error",AngErr);
    Smooth=UserIni->ReadFloat("NUMERIC","Smoothing",Smooth);
	Ngraph=UserIni->ReadInteger("OTHER","Chart Points",Ngraph);
	//Nbars=UserIni->ReadInteger("NUMERIC","Hystogram Bars",Ngraph);
	Nav=UserIni->ReadInteger("NUMERIC","Averaging Points",Nav);
}
//---------------------------------------------------------------------------
int TBeamSolver::GetNumberOfPoints()
{
    return Npoints;
}
/*//---------------------------------------------------------------------------
double TBeamSolver::GetWaveLength()
{
    return lmb;
} */
//---------------------------------------------------------------------------
int TBeamSolver::GetMeshPoints()
{
    return Nmesh;
}
//---------------------------------------------------------------------------
int TBeamSolver::GetNumberOfParticles()
{
	return BeamPar.NParticles;
}
//---------------------------------------------------------------------------
int TBeamSolver::GetNumberOfChartPoints()
{
    return Ngraph;
}
//---------------------------------------------------------------------------
/*int TBeamSolver::GetNumberOfBars()
{
	return Nbars;
}   */
//---------------------------------------------------------------------------
int TBeamSolver::GetNumberOfCells()
{
	return StructPar.NElements;
}
//---------------------------------------------------------------------------
int TBeamSolver::GetNumberOfSections()
{
	return StructPar.NSections;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetInputCurrent()
{
	return BeamPar.Current;
}
//---------------------------------------------------------------------------
TGauss TBeamSolver::GetInputEnergy()
{
	return GetEnergyStats(0);
}
//---------------------------------------------------------------------------
TGauss TBeamSolver::GetInputPhase()
{
	return GetPhaseStats(0);
}
//---------------------------------------------------------------------------
double TBeamSolver::GetInputWavelength()
{
	return GetWavelength(0);
}
//---------------------------------------------------------------------------
TTwiss TBeamSolver::GetInputTwiss(TBeamParameter P)
{
	return GetTwiss(0,P);
}
//---------------------------------------------------------------------------
/*bool TBeamSolver::CheckMagnetization()
{
	return BeamPar.Magnetized;
}    */
//---------------------------------------------------------------------------
bool TBeamSolver::CheckReverse()
{
	return StructPar.Reverse;
}
//---------------------------------------------------------------------------
bool TBeamSolver::CheckDrift(int Nknot)
{
	return Structure[Nknot].drift;
}
//---------------------------------------------------------------------------
TSpaceCharge TBeamSolver::GetSpaceChargeInfo()
{
	return BeamPar.SpaceCharge;
}
//---------------------------------------------------------------------------
TMagnetParameters TBeamSolver::GetSolenoidInfo()
{
	return StructPar.SolenoidPar;
}
//---------------------------------------------------------------------------
bool TBeamSolver::IsKeyWord(AnsiString &S)
{
    return S=="POWER" ||
        S=="SOLENOID" ||
        S=="BEAM" ||
        S=="CURRENT" ||
        S=="DRIFT" ||
        S=="CELL" ||
		S=="CELLS" ||
		S=="SAVE" ||
        S=="OPTIONS" ||
		S=="SPCHARGE";
	  //	S=="COMMENT";
}
//---------------------------------------------------------------------------
TBeamType TBeamSolver::ParseDistribution(AnsiString &S)
{
	 TBeamType D=NOBEAM;
		if (S=="CST_PID")
			D=CST_PID;
		else if (S=="CST_PIT")
			D=CST_PIT;
		else if (S=="TWISS2D")
			D=TWISS_2D;
		else if (S=="TWISS4D")
			D=TWISS_4D;
		else if (S=="SPH2D")
			D=SPH_2D;
		else if (S=="ELL2D")
			D=ELL_2D;
		else if (S=="FILE1D")
			D=FILE_1D;
		else if (S=="FILE2D")
			D=FILE_2D;
		else if (S=="FILE4D")
			D=FILE_4D;
		else if (S=="NORM2D")
			D=NORM_2D;
		/*else if (S=="NORM4D")
			D=NORM_4D;  */
	 return D;
}
//---------------------------------------------------------------------------
bool TBeamSolver::IsFullFileKeyWord(TBeamType D)
{
	bool R=false;

	switch (D) {
		case CST_PID:{}
		case CST_PIT:{R=true;break;}
		default: R=false;
	}
	return R;
}
//---------------------------------------------------------------------------
bool TBeamSolver::IsTransverseKeyWord(TBeamType D)
{
	bool R=false;

	switch (D) {
		case TWISS_2D:{}
		case TWISS_4D:{}
		case FILE_2D:{}
		case FILE_4D:{}
		case TWO_FILES_2D:{}
		case SPH_2D:{}
		case ELL_2D:{R=true;break;}
		default: R=false;
	}
	return R;
}
//---------------------------------------------------------------------------
bool TBeamSolver::IsLongitudinalKeyWord(TBeamType D)
{
	bool R=false;

	switch (D) {
		case FILE_1D:{}
		case FILE_2D:{}
		case NORM_2D:{R=true;break;}
		default: R=false;
	}
	return R;
}
//---------------------------------------------------------------------------
bool TBeamSolver::IsFileKeyWord(TBeamType D)
{
	bool R=false;

	switch (D) {
        case CST_PID:{}
		case CST_PIT: {}
		case FILE_1D:{}
		case FILE_2D:{}
		case TWO_FILES_2D:{}
		case FILE_4D:{R=true;break;}
		default: R=false;
	}
	return R;
}
//---------------------------------------------------------------------------
TInputParameter TBeamSolver::Parse(AnsiString &S)
{
    TInputParameter P;
   // FILE *logFile;

/*    logFile=fopen("BeamSolver.log","a");
	fprintf(logFile,"Parse: S=%s\n",S);
	fclose(logFile); */
	if (S=="POWER")
		P=POWER;
    else if (S=="SOLENOID")
        P=SOLENOID;
    else if (S=="BEAM")
        P=BEAM;
    else if (S=="CURRENT")
		P=CURRENT;
    else if (S=="DRIFT")
        P=DRIFT;
    else if (S=="CELL")
        P=CELL;
    else if (S=="CELLS")
        P=CELLS;
    else if (S=="OPTIONS")
		P=OPTIONS;
	else if (S=="SAVE")
		P=DUMP;
    else if (S=="SPCHARGE")
		P=SPCHARGE;
    else if (S[1]=='!')
		P=COMMENT;
	return  P;
}
//---------------------------------------------------------------------------
TSpaceChargeType TBeamSolver::ParseSpchType(AnsiString &S)
{
	TSpaceChargeType T;;

	if (S=="COULOMB")
		T=SPCH_LPST;
	else if (S=="ELLIPTIC")
		T=SPCH_ELL;
	else if (S=="GWMETHOD")
		T=SPCH_GW;
	else
		T=SPCH_NO;

	return T;
}
//---------------------------------------------------------------------------
void TBeamSolver::GetDimensions(TCell& Cell)
{

    int Nbp=0,Nep=0;
    int Nar=0,Nab=0;
    int Mode=Cell.Mode;

    switch (Mode) {
        case 90:
            Nbp=Nb12;
            Nep=Ne12;
            Nar=Nar23;
            Nab=Nab23;
            break;
        case 120:
            Nbp=Nb23;
            Nep=Ne23;
            Nar=Nar23;
            Nab=Nab23;
            break;
        case 240:
            Nbp=Nb43;
            Nep=Ne43;
            Nar=Nar43;
            Nab=Nab23;
            break;
        default:
            return;
    }

    double *Xo,*Yo,*Xi,*Yi;

    Xo=new double[Nep];
    Yo=new double[Nep];
    Xi=new double[Nbp];
    Yi=new double[Nbp];

    //Searching for a/lmb

  /*FILE *T;
	T=fopen("table.log","a");     */

    for (int i=0;i<Nbp;i++){
        for (int j=0;j<Nep;j++){
            switch (Mode) {
                case 90:
                    Xo[j]=E12[Nbp-i-1][j];
                    Yo[j]=R12[Nbp-i-1][j];
                    break;
                case 120:
                    Xo[j]=E23[Nbp-i-1][j];
                    Yo[j]=R23[Nbp-i-1][j];
                    break;
                case 240:
                    Xo[j]=E43[Nbp-i-1][j];
                    Yo[j]=R43[Nbp-i-1][j];
                    break;
                default:
                    return;
            }
        }
        TSpline Spline;
        Spline.SoftBoundaries=false;
        Spline.MakeLinearSpline(Xo,Yo,Nep);
        Yi[i]=Spline.Interpolate(Cell.ELP);
       //   Xi[i]=mode90?B12[i]:B23[i];
        switch (Mode) {
            case 90:
                Xi[i]=B12[i];
                break;
            case 120:
                Xi[i]=B23[i];
                break;
            case 240:
                Xi[i]=B43[i];
                break;
            default:
                return;
        }
    }

    TSpline rSpline;
    rSpline.SoftBoundaries=false;
    rSpline.MakeLinearSpline(Xi,Yi,Nbp);
    Cell.AkL=rSpline.Interpolate(Cell.betta);

    delete[] Xo;
    delete[] Yo;
    delete[] Xi;
    delete[] Yi;

    Xo=new double[Nar];
    Yo=new double[Nar];
    Xi=new double[Nab];
    Yi=new double[Nab];

    for (int i=0;i<Nab;i++){
        for (int j=0;j<Nar;j++){
            switch (Mode) {
                case 90:
                    Xo[j]=AR[j];
                    Yo[j]=A12[i][j];
                    break;
                case 120:
                    Xo[j]=AR[j];
                    Yo[j]=A23[i][j];
                    break;
                case 240:
                    Xo[j]=AR43[j];
                    Yo[j]=A43[i][j];
                    break;
                default:
                    return;
            }

            /*Xo[j]=AR[j];
            Yo[j]=mode90?A12[i][j]:A23[i][j]; */
        }
        TSpline Spline;
        Spline.SoftBoundaries=false;
        Spline.MakeLinearSpline(Xo,Yo,Nar);
        Yi[i]=Spline.Interpolate(Cell.AkL);
        //Xi[i]=AB[i];
        switch (Mode) {
            case 90:
                Xi[i]=AB[i];
                break;
            case 120:
                Xi[i]=AB[i];
                break;
            case 240:
                Xi[i]=AB[i];
                break;
            default:
                return;
        }
    }

    TSpline aSpline;
    aSpline.SoftBoundaries=false;
    aSpline.MakeLinearSpline(Xi,Yi,Nab);
    Cell.AL32=1e-4*aSpline.Interpolate(Cell.betta);

    delete[] Xo;
    delete[] Yo;
    delete[] Xi;
    delete[] Yi;

  /*    fprintf(T,"%f %f %f %f\n",Cell.betta,Cell.ELP,Cell.AkL,Cell.AL32);
    fclose(T);  */
	//ShowError(E12[0][1]);


  /*    for (int i=0;i<Nbt;i++){
        akl[i]=solve(ELP(akl),ELP=E0,bf[i]);
    }
    akl(bf);
    y=interp(akl(bf),bf);    */


}
//---------------------------------------------------------------------------
TInputLine *TBeamSolver::ParseFile(int& N)
{
	const char *FileName=InputFile.c_str();
	AnsiString S,L,M;

	if (!CheckFile(InputFile)){
		S="ERROR: The input file "+InputFile+" was not found";
		ShowError(S);
		return NULL;
	}

	TInputLine *Lines;
	std::ifstream fs(FileName);
	TInputParameter P;
	int i=-1,j=0,Ns=0;

    FILE *logFile;
	while (!fs.eof()){
		L=GetLine(fs);   //Now reads line by line
		S=ReadWord(L,1);
	   //	S=GetLine(fs);   //Hid common actions inside the function
		if (S=="")
			continue;
		if(IsKeyWord(S) || S[1]=='!'){
			N++;
			if (S=="SAVE") {
				Ns++;
			}
		}
/*        logFile=fopen("BeamSolver.log","a");
		fprintf(logFile,"ParseFile: N=%i, S=%s\n",N,S);
        fclose(logFile); */
    }

    fs.clear();
    fs.seekg(std::ios::beg);

	Lines=new TInputLine[N];
	ResetDump(Ns);

	while (!fs.eof()){
		L=GetLine(fs);   //Now reads line by line
		S=ReadWord(L,1);  //Gets the first word in the line (should be a key word)

		   /*	fs>>s;
			S=AnsiString(s);    */
/*            logFile=fopen("BeamSolver.log","a");
			fprintf(logFile,"ParseFile: s=%s, S=%s\n",s,S);
			fclose(logFile); */
		if (S=="END")
			break;
		else if (S=="") //empty line
			continue;
		else if(IsKeyWord(S) || S[1]=='!'){ //Key word or comment detected. Start parsing the line
			i++;
			P=Parse(S);
/*                logFile=fopen("BeamSolver.log","a");
				fprintf(logFile,"ParseFile: P=%s\n",P);
				fclose(logFile); */
/*				if(S=="COMMENT"){
					logFile=fopen("BeamSolver.log","a");
					fprintf(logFile,"ParseFile: Comment Line\n");
					fclose(logFile);
				}                  */
			Lines[i].P=P;

			if (P==COMMENT)       //comment length is not limited
				Lines[i].S[0]=L;
			else{
				Lines[i].N=NumWords(L)-1; //Parse number of words in the line
				if (Lines[i].N>MaxParameters){
					M="WARNING: Too many parameters in Line "+S+"! The excessive parameters will be ignored";
					Lines[i].N=MaxParameters;
					ShowError(M);
				}

				for (j=0;j<Lines[i].N;j++)
					Lines[i].S[j]=ReadWord(L,j+2);
			}
		}
	}

	fs.close();
    return Lines;
}
//---------------------------------------------------------------------------
AnsiString TBeamSolver::AddLines(TInputLine *Lines,int N1,int N2)
{
	AnsiString S="";

	for (int i=N1; i <= N2; i++) {
    	S+=" \t"+Lines->S[i];
	}

	return S;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParsePID(TInputLine *Line, AnsiString &F)
{
	AnsiString S;

	if (Line->N == 3){
		return ERR_BEAM;
	} else {
		AnsiString FileName=Line->S[1];
		if (CheckFile(FileName)) {
			BeamPar.RFile=FileName;
			F+=AddLines(Line,0,1);
			BeamPar.ZFile=FileName;
			BeamPar.ZBeamType=NORM_1D;

			if (Line->N > 3) {
				BeamPar.ZNorm.mean=DegreeToRad(Line->S[2].ToDouble());
				BeamPar.ZNorm.limit=DegreeToRad(Line->S[3].ToDouble());
				F+=AddLines(Line,2,3);

				if (Line->N > 4) {
					BeamPar.ZNorm.sigma=DegreeToRad(Line->S[4].ToDouble());
					F+=" \t"+Line->S[4];
				} else {
					BeamPar.ZNorm.sigma=100*BeamPar.ZNorm.limit;
				}
			} else {
				BeamPar.ZNorm.mean=pi;
				BeamPar.ZNorm.limit=pi;
				BeamPar.ZNorm.sigma=100*pi;
			}
		} else {
			S="ERROR: The file "+FileName+" is missing!";
			ShowError(S);
			return ERR_BEAM;
		}
	}

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParsePIT(TInputLine *Line, AnsiString &F)
{
	AnsiString FileName=Line->S[1],S;

	if (CheckFile(FileName)) {
		BeamPar.ZBeamType=BeamPar.RBeamType;
		BeamPar.RFile=FileName;
		BeamPar.ZFile=FileName;

		F+=AddLines(Line,0,1);

		if (Line->N == 3){
			if (Line->S[2]=="COMPRESS") {
				BeamPar.ZCompress=true;
				F+=" COMPRESS";
			} else {
				S="ERROR: Wrong keyword "+Line->S[2]+" in BEAM line";
				ShowError(S);
				return ERR_BEAM;
			}
		}
	} else {
		S="ERROR: The file "+FileName+" is missing!";
		ShowError(S);
		return ERR_BEAM;
	}

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseFile2R(TInputLine *Line, AnsiString &F, int Nr)
{
	AnsiString FileName=Line->S[1],S;

	if (CheckFile(FileName)) {
		BeamPar.RFile=FileName;
		F+=AddLines(Line,0,1);
	} else {
		S="ERROR: The file "+FileName+" is missing!";
		ShowError(S);
		return ERR_BEAM;
	}

	if (Nr==2){
		FileName=Line->S[2];
		if (CheckFile(FileName)) {
			BeamPar.YFile=FileName;
			F+=" \t"+FileName;
			BeamPar.RBeamType=TWO_FILES_2D;
		} else {
			S="ERROR: The file "+FileName+" is missing!";
			ShowError(S);
			return ERR_BEAM;
		}
	}
	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseTwiss2D(TInputLine *Line, AnsiString &F, int Nr)
{
	AnsiString S;

	if (Nr != 3){
		S="ERROR: Wrong number of Twiss parameters in BEAM line!";
		ShowError(S);
		return ERR_BEAM;
	}else{
		BeamPar.XTwiss.alpha=Line->S[1].ToDouble();
		BeamPar.XTwiss.beta=Line->S[2].ToDouble()/100; //m/rad
		BeamPar.XTwiss.epsilon=Line->S[3].ToDouble()/100; //m*rad
		BeamPar.YTwiss=BeamPar.XTwiss;
		F+=AddLines(Line,0,3);
	}
	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseTwiss4D(TInputLine *Line, AnsiString &F, int Nr)
{
	AnsiString S;

	if (Nr != 6){
		S="ERROR: Wrong number of Twiss parameters in BEAM line!";
		ShowError(S);
		return ERR_BEAM;
	}
	else{
		BeamPar.XTwiss.alpha=Line->S[1].ToDouble();
		BeamPar.XTwiss.beta=Line->S[2].ToDouble()/100; //cm/rad
		BeamPar.XTwiss.epsilon=Line->S[3].ToDouble()/100; //cm*rad
		BeamPar.YTwiss.alpha=Line->S[4].ToDouble();
		BeamPar.YTwiss.beta=Line->S[5].ToDouble()/100;
		BeamPar.YTwiss.epsilon=Line->S[6].ToDouble()/100;
		F+=AddLines(Line,0,6);
	}

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseSPH(TInputLine *Line, AnsiString &F, int Nr)
{
	AnsiString S;

    if (Nr>3){
		S="ERROR: Too many Spherical parameters in BEAM line!";
		ShowError(S);
		return ERR_BEAM;
	} else{
		BeamPar.Sph.Rcath=Line->S[1].ToDouble()/100; //Rcath cm
		F+=AddLines(Line,0,1);
		if (Nr>1) {
			BeamPar.Sph.Rsph=Line->S[2].ToDouble()/100;  //Rsph cm
			F+=" \t"+Line->S[1];
			if (Nr>2){
				BeamPar.Sph.kT=Line->S[3].ToDouble(); //kT
				F+=" \t"+Line->S[1];
			} else
				BeamPar.Sph.kT=0;
		} else {
			BeamPar.Sph.Rsph=0;
			BeamPar.Sph.kT=0;
		}
	}

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseELL(TInputLine *Line, AnsiString &F, int Nr)
{
	AnsiString S;

	if (Nr>4){
		S="ERROR: Too many Elliptical parameters in BEAM line!";
		ShowError(S);
		return ERR_BEAM;
	}else{
		BeamPar.Ell.ax=Line->S[1].ToDouble()/100; //x cm
		F+=AddLines(Line,0,1);
		if (Nr>1) {
			BeamPar.Ell.by=Line->S[2].ToDouble()/100; //y cm
			F+=" \t"+Line->S[2];
			if (Nr>2){
				BeamPar.Ell.phi=DegreeToRad(Line->S[3].ToDouble()); //phi
				F+=" \t"+Line->S[3];
				if (Nr>3){
					BeamPar.Ell.h=Line->S[4].ToDouble(); //h
					F+=" \t"+Line->S[4];
				} else
					BeamPar.Ell.h=1;
			} else {
				BeamPar.Ell.phi=0;
				BeamPar.Ell.h=1;
			}
		} else {
			BeamPar.Ell.by=BeamPar.Ell.ax; //y
			BeamPar.Ell.phi=0;
		 	BeamPar.Ell.h=1;
		}
	}
	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseFile1Z(TInputLine *Line, AnsiString &F, int Nz,int Zpos)
{
	AnsiString S;

	if (Nz != 3 && Nz != 4){
		S="ERROR: Wrong number of Import File (Z) or Phase Distribution parameters in BEAM line!";
		ShowError(S);
		return ERR_BEAM;
	}

	AnsiString FileName=Line->S[Zpos+1];
	if (CheckFile(FileName)) {
		BeamPar.ZFile=FileName;
		F+=AddLines(Line,Zpos,Zpos+1);
	} else {
		S="ERROR: The file "+FileName+" is missing!";
		ShowError(S);
		return ERR_BEAM;
	}

	BeamPar.ZNorm.mean=DegreeToRad(Line->S[Zpos+2].ToDouble());
	BeamPar.ZNorm.limit=DegreeToRad(Line->S[Zpos+3].ToDouble());
	F+=AddLines(Line,Zpos+2,Zpos+3);

	if (Nz == 3) {
		BeamPar.ZNorm.sigma=100*BeamPar.ZNorm.limit;
	} else  if (Nz == 4) {
		BeamPar.ZNorm.sigma=DegreeToRad(Line->S[Zpos+4].ToDouble());
		F+=Line->S[Zpos+4];
	}

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseFile2Z(TInputLine *Line, AnsiString &F, int Nz,int Zpos)
{
	AnsiString S;

	if (Nz != 1){
		S="ERROR: Too many Import File (Z) parameters in BEAM line!";
		ShowError(S);
		return ERR_BEAM;
	}

	AnsiString FileName=Line->S[Zpos+1];
	if (CheckFile(FileName)) {
		BeamPar.ZFile=FileName;
		F+=AddLines(Line,Zpos,Zpos+1);
	} else {
		S="ERROR: The file "+FileName+" is missing!";
		ShowError(S);
		return ERR_BEAM;
	}

	return ERR_NO;
}
 //---------------------------------------------------------------------------
TError TBeamSolver::ParseNorm(TInputLine *Line, AnsiString &F, int Nz,int Zpos)
{
	AnsiString S;

	if (Nz != 4 && Nz != 6){
		S="Wrong number of Longitudinal Distribution parameters in BEAM line!";
		ShowError(S);
		return ERR_BEAM;
	}else{
		if (Nz == 4) {
			BeamPar.WNorm.mean=Line->S[Zpos+1].ToDouble();
			BeamPar.WNorm.limit=Line->S[Zpos+2].ToDouble();
			BeamPar.WNorm.sigma=100*BeamPar.WNorm.limit;
			BeamPar.ZNorm.mean=DegreeToRad(Line->S[Zpos+3].ToDouble());
			BeamPar.ZNorm.limit=DegreeToRad(Line->S[Zpos+4].ToDouble());
			BeamPar.ZNorm.sigma=100*BeamPar.ZNorm.limit;
			F+=AddLines(Line,Zpos,Zpos+4);
		} else  if (Nz == 6) {
			BeamPar.WNorm.mean=Line->S[Zpos+1].ToDouble();
			BeamPar.WNorm.limit=Line->S[Zpos+2].ToDouble();
			BeamPar.WNorm.sigma=Line->S[Zpos+3].ToDouble();
			BeamPar.ZNorm.mean=DegreeToRad(Line->S[Zpos+4].ToDouble());
			BeamPar.ZNorm.limit=DegreeToRad(Line->S[Zpos+5].ToDouble());
			BeamPar.ZNorm.sigma=DegreeToRad(Line->S[Zpos+6].ToDouble());
			F+=AddLines(Line,Zpos,Zpos+6);
		}
	}

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseOptions(TInputLine *Line)
{
	TError Error=ERR_NO;
	AnsiString F="OPTIONS ";

	for (int j=0;j<Line->N;j++){
		if (Line->S[j]=="REVERSE")
			StructPar.Reverse=true;

	/*	if (Line->S[j]=="MAGNETIZED")
			BeamPar.Magnetized=true;   */

		F=F+"\t"+Line->S[j];
	}
	ParsedStrings->Add(F);

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseSpaceCharge(TInputLine *Line)
{
	AnsiString F="SPCHARGE ",S;
	BeamPar.SpaceCharge.Type=SPCH_NO;
	BeamPar.SpaceCharge.NSlices=1;
	BeamPar.SpaceCharge.Nrms=3;

	if (Line->N==0){
		BeamPar.SpaceCharge.Type=SPCH_ELL;
		F=F+"\t"+"ELLIPTIC";
	}else if (Line->N<3) {
		BeamPar.SpaceCharge.Type=ParseSpchType(Line->S[0]);
		F=F+"\t"+Line->S[0];
		switch (BeamPar.SpaceCharge.Type) {
			case SPCH_GW: {
				if (Line->N==2){
					BeamPar.SpaceCharge.NSlices=Line->S[1].ToInt();
					F=F+"\t"+Line->S[1];
				}
				break;
			}
			case SPCH_LPST: { }
			case SPCH_ELL: {
				if (Line->N==2){
					BeamPar.SpaceCharge.Nrms=Line->S[1].ToDouble();
					F=F+"\t"+Line->S[1];
				}
				break;
			}
			case SPCH_NO: { break;}
			default: {return ERR_SPCHARGE;};
		}

	} else {
		S="ERROR: Too many parametes in SPCHARGE line!";
		ShowError(S);
		return ERR_SPCHARGE;
	}

	ParsedStrings->Add(F);
	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseSolenoid(TInputLine *Line)
{
	AnsiString F="SOLENOID",SolenoidFile,S;

	if (Line->N==3){
		StructPar.SolenoidPar.ImportType=ANALYTIC_0D;
		StructPar.SolenoidPar.BField=Line->S[0].ToDouble()/10000; //[Gs]
		StructPar.SolenoidPar.Length=Line->S[1].ToDouble()/100; //[cm]
		StructPar.SolenoidPar.StartPos=Line->S[2].ToDouble()/100; //[cm]

		F+=AddLines(Line,0,2);
		ParsedStrings->Add(F);
	}else if (Line->N==1) {
		SolenoidFile=Line->S[0];
		F+="\t"+SolenoidFile;
		if (CheckFile(SolenoidFile)){
			StructPar.SolenoidPar.ImportType=IMPORT_1D;
			StructPar.SolenoidPar.File=SolenoidFile;
			ParsedStrings->Add(F);
		}else{
			S="ERROR: The file "+SolenoidFile+" is missing!";
			ShowError(S);
			return ERR_SOLENOID;
		}
	}else
		return ERR_SOLENOID;

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseBeam(TInputLine *Line)
{
	TError Error=ERR_NO;
	AnsiString F="BEAM",S;

	if (Line->N < 1)
		return ERR_BEAM;

	AnsiString KeyWord=Line->S[0];
	BeamPar.RBeamType=ParseDistribution(KeyWord);
	BeamPar.ZCompress=false;

	if (BeamPar.RBeamType==NOBEAM) {
		S="ERROR: Wrong KEYWORD in BEAM line";
		ShowError(S);
		return ERR_BEAM;
	 }

	 if (IsFullFileKeyWord(BeamPar.RBeamType)) {
		if (Line->N < 2 || Line->N >5)
			return ERR_BEAM;

			switch (BeamPar.RBeamType) {
				case CST_PID: {
					Error=ParsePID(Line,F);
					break;
				}
				case CST_PIT: {
					Error=ParsePIT(Line,F);
					break;
				}
				default: {
					S="ERROR: Unexpected Input File Type";
					ShowError(S);
					return ERR_BEAM;
				}
			}
	 } else if (IsTransverseKeyWord(BeamPar.RBeamType)) {
		int Zpos=0;
		for (int j=1; j < Line->N; j++) {
			if (IsLongitudinalKeyWord(ParseDistribution(Line->S[j]))) {
				Zpos=j;
				break;
			}
		}
		if (Zpos==0) {
			S="Longitudinal Distribution Type is not defined in BEAM line";
			ShowError(S);
			return ERR_BEAM;
		}
		int Nr=Zpos-1; // Number of R-parameters
		int Nz=Line->N-Zpos-1; //Number of Z-parameters

		if (Nr==0 || Nz==0) {
			S="Too few parametes in BEAM line!";
			ShowError(S);
			return ERR_BEAM;
		}

		switch (BeamPar.RBeamType) {
			case FILE_4D: {
				if (Nr!=1){
					S="Too many Import File (R) parameters in BEAM line!";
					ShowError(S);
					return ERR_BEAM;
				}
			}
			case FILE_2D: {
				if (Nr>2){
					S="Too many Import File (R) parameters in BEAM line!";
					ShowError(S);
					return ERR_BEAM;
				}
				Error=ParseFile2R(Line,F,Nr);
				break;
			}
			case TWISS_2D: {
				Error=ParseTwiss2D(Line,F,Nr);
				break;
			}
			case TWISS_4D: {
				Error=ParseTwiss4D(Line,F,Nr);
				break;
			}
			case SPH_2D: {
				Error=ParseSPH(Line,F,Nr);
				break;
			}
			case ELL_2D: {
				Error=ParseELL(Line,F,Nr);
				break;
			}
			default: {
				S="Unexpected BEAM Transversal Distribution Type";
				ShowError(S);
				return ERR_BEAM;
			}
		}
		KeyWord=Line->S[Zpos];
		BeamPar.ZBeamType=ParseDistribution(KeyWord);

		if (IsLongitudinalKeyWord(BeamPar.ZBeamType)) {
			switch (BeamPar.ZBeamType) {
				case FILE_1D: {
					Error=ParseFile1Z(Line,F,Nz,Zpos);
					break;
				}
				case FILE_2D: {
					Error=ParseFile2Z(Line,F,Nz,Zpos);
					break;
				}
				case NORM_2D: {
					Error=ParseNorm(Line,F,Nz,Zpos);
					break;
				}
				default: {
					S="ERROR:Unexpected BEAM Longitudinal Distribution Type";
					ShowError(S);
					return ERR_BEAM;
				}
			}
		} else{
			S="ERROR: Unexpected BEAM Longitudinal Distribution Type";
			ShowError(S);
			return ERR_BEAM;
		}
	 }  else  {
		S="ERROR: Wrong Format of BEAM line";
		ShowError(S);
		return ERR_BEAM;
	 }
	 ParsedStrings->Add(F);

	 return Error;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseCurrent(TInputLine *Line)
{
	AnsiString F="CURRENT",S;
	if (Line->N<1 || Line->N>2){
		return ERR_CURRENT;
	}  else {
		BeamPar.Current=Line->S[0].ToDouble();
		F+="\t"+Line->S[0];
		if (!IsFileKeyWord(BeamPar.RBeamType) && !IsFileKeyWord(BeamPar.ZBeamType)) {
			if (Line->N!=2){
				S="ERROR: Number of particles is not defined!";
				ShowError(S);
				return ERR_CURRENT;
			} else {
				BeamPar.NParticles=Line->S[1].ToInt();
				F+="\t"+Line->S[1];
			}
		} else {
			int Nr=0, Ny=0, Nz=0, Np=0;

			if (IsFullFileKeyWord(BeamPar.RBeamType)){
				switch (BeamPar.RBeamType) {
					case CST_PID:{
						Nr=NumPointsInFile(BeamPar.RFile,PID_LENGTH);
						break;
					}
					case CST_PIT:{
						Nr=NumPointsInFile(BeamPar.RFile,PIT_LENGTH);
						break;
					}
					default:  {
						S="ERROR: Unexpected BEAM File Format!";
						ShowError(S);
						return ERR_IMPORT;
					}
				}

				Nz=Nr;
			} else {
				switch (BeamPar.RBeamType) {
					case FILE_2D: {
						Nr=NumPointsInFile(BeamPar.RFile,2);
						break;
					}
					case TWO_FILES_2D: {
						Nr=NumPointsInFile(BeamPar.RFile,2);
						Ny=NumPointsInFile(BeamPar.YFile,2);
						if (Nr!=Ny) {
							S="ERROR: The numbers of imported particles from two Transverse Beam Files don't match!";
							ShowError(S);
							return ERR_IMPORT;
						}
						break;
					}
					case FILE_4D: {
						Nr=NumPointsInFile(BeamPar.RFile,4);
						break;
					}
					default:  {
						Nr=-1;
						break;
					}
				}

				switch (BeamPar.ZBeamType) {
					case FILE_1D: {
						Nz=NumPointsInFile(BeamPar.ZFile,1);
						break;
					}
					case FILE_2D: {
						Nz=NumPointsInFile(BeamPar.ZFile,2);
						break;
					}
					default:  {
						Nz=Nr;
						break;
					}
				}

				if (Nr==-1)
					Nr=Nz;


			}
			//check Np mismatch
			if (IsFileKeyWord(BeamPar.RBeamType) && IsFileKeyWord(BeamPar.ZBeamType)){
				if (Nr!=Nz) {
					S="ERROR: The number of imported particles from Longintudinal Beam File doesn't match with the number of particles imported from Transversal Beam File!";
					ShowError(S);
					return ERR_IMPORT;
				}
			}

			//check Np overrride
			if (Line->N==2) {
				Np=Line->S[1].ToInt();

				if (Np>Nr) {
					S="WARNING: The number of defined particles exceeds the number of imported particles, and will be ignored!";
					ShowError(S);
				}  else if (Np<Nr) {
					S="WARNING: The number of defined particles is less than the number of imported particles! Only the first "+Line->S[1]+" particles will be simulated";
					ShowError(S);
					Nr=Np;
				}
				F+=" "+Line->S[1];
			}
			BeamPar.NParticles=Nr;
		}
	}

	ParsedStrings->Add(F);

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseCell(TInputLine *Line,int Ni, int Nsec, bool NewCell)
{
	if (Line->N==3 || Line->N==5){
		StructPar.Cells[Ni].Mode=Line->S[0].ToDouble();
		StructPar.Cells[Ni].betta=Line->S[1].ToDouble();
		StructPar.Cells[Ni].ELP=Line->S[2].ToDouble();
		StructPar.Cells[Ni].Mesh=Nmesh;

		if (Line->N==3){
			GetDimensions(StructPar.Cells[Ni]);
		} else if (Line->N==5){
			StructPar.Cells[Ni].AL32=Line->S[3].ToDouble();
			StructPar.Cells[Ni].AkL=Line->S[4].ToDouble();
		}
	 } else{
		return ERR_CELL;
	 }

	 StructPar.Cells[Ni].F0=StructPar.Sections[Nsec-1].Frequency;
	 StructPar.Cells[Ni].P0=StructPar.Sections[Nsec-1].Power;
	 StructPar.Cells[Ni].First=NewCell;
	 StructPar.Cells[Ni].dF=NewCell?StructPar.Sections[Nsec-1].PhaseShift:0;
	 StructPar.Sections[Nsec-1].NCells++;

	 return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseSingleCell(TInputLine *Line,int Ni, int Nsec, bool NewCell)
{
	 AnsiString F="CELL ",S;
	 TError Err;

	 Err=ParseCell(Line,Ni,Nsec,NewCell);

	 F+=Line->S[0]+"\t"+S.FormatFloat("#0.000",StructPar.Cells[Ni].betta)+"\t"+S.FormatFloat("#0.000",StructPar.Cells[Ni].ELP)+"\t"+S.FormatFloat("#0.000000",StructPar.Cells[Ni].AL32)+"\t"+S.FormatFloat("#0.000000",StructPar.Cells[Ni].AkL);
	 ParsedStrings->Add(F);

	 return Err;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseMultipleCells(TInputLine *Line,int Ni, int Nsec, bool NewCell)
{
	AnsiString F="CELLS ",S;

	TError Err;
	TInputLine ParsedLine;
	ParsedLine=*Line;
	int Ncells=Line->S[0].ToInt();
	for (int i = 0; i < Line->N-1; i++) {
		ParsedLine.S[i]=Line->S[i+1];
	}
	ParsedLine.N=Line->N-1;

	for (int i = 0; i < Ncells; i++) {
		Err=ParseCell(&ParsedLine, Ni+i,Nsec,NewCell);
		NewCell=false;
	}

	F+=Line->S[0]+"\t"+Line->S[1]+"\t"+S.FormatFloat("#0.000",StructPar.Cells[Ni].betta)+"\t"+S.FormatFloat("#0.000",StructPar.Cells[Ni].ELP)+"\t"+S.FormatFloat("#0.000000",StructPar.Cells[Ni].AL32)+"\t"+S.FormatFloat("#0.000000",StructPar.Cells[Ni].AkL);
	ParsedStrings->Add(F);

	return Err;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseDrift(TInputLine *Line,int Ni, int Nsec)
{
	AnsiString F="DRIFT ",S;
	double m=0;
	TError Err;

	if (Line->N<2 || Line->N>3){
		return ERR_DRIFT;
	} else {
		StructPar.Cells[Ni].Drift=true;
		StructPar.Cells[Ni].betta=Line->S[0].ToDouble()/100;//D, cm
		StructPar.Cells[Ni].AkL=Line->S[1].ToDouble()/100;//Ra, cm
		if (Line->N==3){
			m=Line->S[2].ToDouble();
			if (m>1)
				StructPar.Cells[Ni].Mesh=m;
			else{
				S="WARNING: Number of mesh points must be more than 1";
				ShowError(S);
				StructPar.Cells[Ni].Mesh=Nmesh;
			}
		} else
			StructPar.Cells[Ni].Mesh=Nmesh;

		StructPar.Cells[Ni].ELP=0;
		StructPar.Cells[Ni].AL32=0;
		StructPar.Cells[Ni].First=true;
		StructPar.Cells[Ni].F0=c;
		StructPar.Cells[Ni].dF=arc(StructPar.Sections[Nsec-1].PhaseShift);

		F+=Line->S[0]+" \t"+Line->S[1];
		ParsedStrings->Add(F);
	}

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParsePower(TInputLine *Line,int Nsec)
{
	AnsiString F="POWER ",S;
	if (Line->N>=2 && Line->N<=3){
		if (Nsec>StructPar.NSections) {
			S="ERROR: Inconsistent number of sections!";
			ShowError(S);
			return ERR_FORMAT;
		}  else{
			StructPar.Sections[Nsec].Power=1e6*Line->S[0].ToDouble();
			StructPar.Sections[Nsec].Frequency=1e6*Line->S[1].ToDouble();
			StructPar.Sections[Nsec].NCells=0;
			F+=Line->S[0]+"\t"+Line->S[1]+"\t";
			if(Line->N==3){
				StructPar.Sections[Nsec].PhaseShift=arc(Line->S[2].ToDouble());
				F+=Line->S[2];
			}else
				StructPar.Sections[Nsec].PhaseShift=0;
		}
	} else
		return ERR_COUPLER;

	ParsedStrings->Add(F);

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseDump(TInputLine *Line, int Ns, int Ni)
{
	bool DefaultSet=true;
	int Nkey=1; // the word from which the flags are checked
	AnsiString F="SAVE ",F0, S;

	if (Line->N>=1 && Line->N<=9){
		BeamExport[Ns].NElement=Ni;
		BeamExport[Ns].File=Line->S[0].c_str();
		BeamExport[Ns].LiveOnly=true;
		BeamExport[Ns].SpecialFormat=NOBEAM;
		BeamExport[Ns].Phase=false;
		BeamExport[Ns].Energy=false;
		BeamExport[Ns].Radius=false;
		BeamExport[Ns].Azimuth=false;
		BeamExport[Ns].Divergence=false;

		F+=Line->S[0];
		F0=F;
			if (Line->N>=2 && IsNumber(Line->S[1])) {
				BeamExport[Ns].N1=Line->S[1].ToDouble();
				F=F+" "+Line->S[1];
				Nkey=2;
				if (Line->N>=3 && IsNumber(Line->S[2])){
					BeamExport[Ns].N2=Line->S[2].ToDouble();
					F=F+" "+Line->S[2];
					Nkey=3;
				} else
					BeamExport[Ns].N2=0;
			}  else {
				BeamExport[Ns].N1=0;
				BeamExport[Ns].N2=0;
			}
			if (Line->N>=2 && Line->N<=9) {
				for (int j=Nkey;j<Line->N;j++){
					if (Line->S[j]=="PID") {
						BeamExport[Ns].SpecialFormat=CST_PID;
						F=F0+" PID";
						DefaultSet=false;
						break;
					}
					else if (Line->S[j]=="PIT") {
						BeamExport[Ns].SpecialFormat=CST_PIT;
						F=F0+" PIT";
						DefaultSet=false;
						break;
					}
					else if (Line->S[j]=="ASTRA") {
						BeamExport[Ns].SpecialFormat=ASTRA;
						F=F0+" ASTRA";
						DefaultSet=false;
						break;
					}
					else if (Line->S[j]=="LOST") {
						BeamExport[Ns].LiveOnly=false;
						F=F+" LOST";
					}
					else if (Line->S[j]=="PHASE") {
						BeamExport[Ns].Phase=true;
						F=F+" PHASE";
						DefaultSet=false;
					}
					else if (Line->S[j]=="ENERGY") {
						BeamExport[Ns].Energy=true;
						F=F+" ENERGY";
						DefaultSet=false;
					}
					else if (Line->S[j]=="RADIUS") {
						BeamExport[Ns].Radius=true;
						F=F+" RADIUS";
						DefaultSet=false;
					}
					else if (Line->S[j]=="AZIMUTH") {
						BeamExport[Ns].Azimuth=true;
						F=F+" AZIMUTH";
						DefaultSet=false;
					}
					else if (Line->S[j]=="DIVERGENCE") {
						BeamExport[Ns].Divergence=true;
						F=F+" DIVERGENCE";
						DefaultSet=false;
					} else {
						S="WARNING: The keyword "+Line->S[j]+" in SAVE line is not recognized and will be ignored";
						ShowError(S);
					}
				}
			}

			if (DefaultSet) {
				BeamExport[Ns].Phase=true;
				BeamExport[Ns].Energy=true;
				BeamExport[Ns].Radius=true;
				BeamExport[Ns].Azimuth=true;
				BeamExport[Ns].Divergence=true;
			}

		 /*	if (BeamExport[Ns].SpecialFormat!=NOBEAM) {
				BeamExport[Ns].Phase=false;
				BeamExport[Ns].Energy=false;
				BeamExport[Ns].Radius=false;
				BeamExport[Ns].Azimuth=false;
				BeamExport[Ns].Divergence=false;
			}         */

			//check the numbers
			if (BeamExport[Ns].N1<0 || BeamExport[Ns].N2<0) {
				S="WARNING: The particle numbers in SAVE line can not be negative. The region will be ignored.";
				ShowError(S);
				BeamExport[Ns].N1=0;
				BeamExport[Ns].N2=0;
			} else if (BeamExport[Ns].N2!=0 && BeamExport[Ns].N1>BeamExport[Ns].N2) {
				int temp=BeamExport[Ns].N2; //switch the numbers
				BeamExport[Ns].N2=BeamExport[Ns].N1;
				BeamExport[Ns].N1=temp;
			}
		ParsedStrings->Add(F);
	} else {    // else skip the line
		S="WARNING: The format of SAVE line is wrong and will be ignored.";
		ShowError(S);
	}

	return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::ParseLines(TInputLine *Lines,int N,bool OnlyParameters)
{
	int Ni=0, Nsec=0, Ns=0;
	double dF=0;
	double F_last=0, P_last=0;

	TError Error=ERR_NO;

	bool BeamDefined=false;
	bool CurrentDefined=false;
	bool CellDefined=false;
	bool PowerDefined=false;

	bool NewCell=true;
	AnsiString F,S,s;
    ParsedStrings->Clear();

   // FILE *logFile;
	for (int k=0;k<N;k++){
	 /*   logFile=fopen("BeamSolver.log","a");
        fprintf(logFile,"ParseLines: Line %i \n",k);
		fclose(logFile);     */
		switch (Lines[k].P) {
			case UNDEFINED: {
				throw std::logic_error("Unhandled TInputParameter UNDEFINED in TBeamSolver::ParseLines");
			}
			case OPTIONS:{
				Error=ParseOptions(&Lines[k]);
				break;
			}
			case SPCHARGE:{
				Error=ParseSpaceCharge(&Lines[k]);
				break;
			}
			case SOLENOID:{
				Error=ParseSolenoid(&Lines[k]);
				break;
			}
			case BEAM:{
				Error=ParseBeam(&Lines[k]);
				BeamDefined=Error==ERR_NO;
				break;
			}
			case CURRENT:{
				Error=ParseCurrent(&Lines[k]);
				CurrentDefined=Error==ERR_NO;
				break;
			}
			case CELL:{ }
			case CELLS:{
				if(!OnlyParameters){
					if (NewCell && !PowerDefined) {
						S="ERROR: The RF power source must be defined before each RF section. Put POWER line in correct format into the input file before the end of each RF section. Note that DRIFT element terminates the defined RF power!";
						ShowError(S);
						return ERR_FORMAT;
					}
					if (Lines[k].P==CELL) {
						Error=ParseSingleCell(&Lines[k],Ni,Nsec,NewCell);
						Ni++;
					} else if (Lines[k].P==CELLS) {
						Error=ParseMultipleCells(&Lines[k],Ni,Nsec,NewCell);
						Ni+=Lines[k].S[0].ToInt();
					}

					CellDefined=Error==ERR_NO;

					NewCell=false;

					if (StructPar.ElementsLimit>-1 &&Ni>=StructPar.ElementsLimit)
						OnlyParameters=true;
				}

				break;
			}
			case DRIFT:{
				if(!OnlyParameters){
					Error=ParseDrift(&Lines[k],Ni,Nsec);
					NewCell=true;
					PowerDefined=false;
					Ni++;
				}

				if (StructPar.ElementsLimit>-1 && Ni>=StructPar.ElementsLimit)
					OnlyParameters=true;

				break;
			}
			case POWER:{
				Error=ParsePower(&Lines[k],Nsec);
				if (Error==ERR_NO){
				   	Nsec++;
					NewCell=true;
					PowerDefined=true;
				} else
					return ERR_COUPLER;
				break;
			}
			case DUMP:{
				Error=ParseDump(&Lines[k],Ns,Ni);
				Ns++;
				break;
			}
			case COMMENT:{
				ParsedStrings->Add(Lines[k].S[0]);
				break;
			}
		}

		if (Error!=ERR_NO)
			return Error;
	}

	if (!BeamDefined) {
		S="ERROR: The beam particles distribution not defined. Put BEAM line in correct format into the input file";
		ShowError(S);
		Error=ERR_FORMAT;
	}
	if (!CurrentDefined) {
		S="ERROR: The beam current is not defined. Put CURRENT line in correct format into the input file";
		ShowError(S);
		Error=ERR_FORMAT;
	}
	if (Ni<1) {
		S="ERROR: The accelerating structure is not defined. Put at least one element (CELL, DRIFT) line in correct format into the input file";
		ShowError(S);
		Error=ERR_FORMAT;
	}
   /*	if (CellDefined && !PowerDefined) {
		ShowError("ERROR: The RF power source must be defined before each RF section. Put POWER line in correct format into the input file before the end of each RF section");
		Error=ERR_FORMAT;
	}       */

	return Error;
}
//---------------------------------------------------------------------------
TError TBeamSolver::LoadData(int Nlim)
{
    const char *FileName=InputFile.c_str();
    LoadIniConstants();
    InputStrings->Clear();
	StructPar.ElementsLimit=Nlim; //# of cells limit. Needed for optimizer. Don't remove

    DataReady=false;

    if (strncmp(FileName, "", 1) == 0) {
        return ERR_NOFILE;
    }
    if (!FileExists(FileName)) {
        return ERR_OPENFILE;
    }

	AnsiString S;
    TInputLine *Lines;
    int i=-1,j=0,N=0;
    TError Parsed;

    Lines=ParseFile(N);
    //InputStrings->LoadFromFile(InputFile);

	ResetStructure();

	for (int k=0;k<N;k++){
		if (Lines[k].P==CELL || Lines[k].P==DRIFT)
			StructPar.NElements++;
		else if (Lines[k].P==CELLS)
			StructPar.NElements+=Lines[k].S[0].ToInt();
		else if (Lines[k].P==POWER)
			StructPar.NSections++;
	}

	if (StructPar.ElementsLimit>-1 && StructPar.NElements>=StructPar.ElementsLimit){  //# of cells limit. Needed for optimizer
		StructPar.NElements=StructPar.ElementsLimit;
		S="The number of elements exceeds the user-defined limit. Check the hellweg.ini file";
		ShowError(S);
	}


	StructPar.Cells = new TCell[StructPar.NElements+1];  //+1 - end point
	for (int k=0;k<StructPar.NElements+1;k++){
		//StructPar.Cells[k].Dump=false;
		StructPar.Cells[k].Drift=false;
		StructPar.Cells[k].First=false;
	}

	if (StructPar.NSections==0){
		StructPar.NSections=1; //default section!
		StructPar.Sections=new TSectionParameters [StructPar.NSections];
		//StructPar.Sections[0].Wavelength=1;
		StructPar.Sections[0].Frequency=c;
		StructPar.Sections[0].Power=0;
		StructPar.Sections[0].NCells=0;
	} else
		StructPar.Sections=new TSectionParameters [StructPar.NSections];

	Parsed=ParseLines(Lines,N);
    ParsedStrings->Add("END");
    InputStrings->AddStrings(ParsedStrings);

    ParsedStrings->SaveToFile("PARSED.TXT");

    delete[] Lines;
    DataReady=true;
    return Parsed;
}
//---------------------------------------------------------------------------
TError TBeamSolver::MakeBuncher(TCell& iCell)
{
    const char *FileName=InputFile.c_str();
    InputStrings->Clear();
    //LoadIniConstants();

    DataReady=false;
    TError Error;

    if (strncmp(FileName, "", 1) == 0) {
        return ERR_NOFILE;
    }
    if (!FileExists(FileName)) {
        return ERR_OPENFILE;
    }

	ResetStructure();

    AnsiString F,s;
    TInputLine *Lines;
    int i=-1,j=0,N=0;

    Lines=ParseFile(N);
    Error=ParseLines(Lines,N,true);

	//NOT CLEAR. MAY NEED TO UNCOMMENT AND REWRITE
   if (StructPar.Sections[0].Frequency*1e6!=iCell.F0 || StructPar.Sections[0].Power*1e6!=iCell.P0){
		StructPar.Sections[0].Frequency=iCell.F0*1e-6;
		StructPar.Sections[0].Power=iCell.P0*1e-6;
		F="POWER "+s.FormatFloat("#0.00",StructPar.Sections[0].Power)+"\t"+s.FormatFloat("#0.00",StructPar.Sections[0].Frequency);
        ParsedStrings->Add(F);
	}

    ParsedStrings->Add("");

    double k1=0,k2=0,k3=0,k4=0,k5=0;
	double Am=iCell.ELP*sqrt(StructPar.Sections[0].Power*1e6)/We0;
    k1=3.8e-3*(Power(10.8,Am)-1);
    k2=1.25*Am+2.25;
    k3=0.5*Am+0.15*sqrt(Am);
    k4=0.5*Am-0.15*sqrt(Am);
    k5=1/sqrt(1.25*sqrt(Am));

	double b=0,A=0,ksi=0,lmb=0,th=0;
	double W0=Beam[0]->GetAverageEnergy();
	double b0=MeVToVelocity(W0);

	lmb=1e-6*c/StructPar.Sections[0].Frequency;
    if (iCell.Mode==90)
        th=pi/2;
    else if (iCell.Mode==120)
        th=2*pi/3;

	StructPar.NElements=0;
	StructPar.NSections=1;

	TSectionParameters Sec;
	Sec=StructPar.Sections[0];
	delete [] StructPar.Sections;
	StructPar.Sections=new TSectionParameters [StructPar.NSections];
	StructPar.Sections[0]=Sec;
   //	Ncells=0;
    int iB=0;

    do {
        b=(2/pi)*(1-b0)*arctg(k1*Power(ksi,k2))+b0;
       //   b=(2/pi)*arctg(0.25*sqr(10*ksi*lmb)+0.713);
		iB=10000*b;
        ksi+=b*th/(2*pi);
		StructPar.NElements++;
    } while (iB<9990);
    //} while (ksi*lmb<1.25);

	StructPar.Cells=new TCell[StructPar.NElements];

    ksi=0;
	for (int i=0;i<StructPar.NElements;i++){
        ksi+=0.5*b*th/(2*pi);

        b=(2/pi)*(1-b0)*arctg(k1*Power(ksi,k2))+b0;
       //   b=(2/pi)*arctg(0.25*sqr(10*ksi*lmb)+0.713);

        A=k3-k4*cos(pi*ksi/k5);
        //A=3.0e6*sin(0.11*sqr(10*ksi*lmb)+0.64);
       /*
        if (ksi*lmb>0.3)
            A=3.00e6;     */

        if (ksi>k5)
            A=Am;

        ksi+=0.5*b*th/(2*pi);

		StructPar.Cells[i].Mode=iCell.Mode;
		StructPar.Cells[i].betta=(2/pi)*(1-b0)*arctg(k1*Power(ksi,k2))+b0;
        //Cells[i].betta=(2/pi)*arctg(0.25*sqr(10*ksi*lmb)+0.713);

		StructPar.Cells[i].ELP=A*We0/sqrt(1e6*StructPar.Sections[0].Power);
       //   Cells[i].ELP=A*lmb/sqrt(1e6*P0);

		GetDimensions(StructPar.Cells[i]);

		F="CELL "+s.FormatFloat("#0",iCell.Mode)+"\t"+s.FormatFloat("#0.000",StructPar.Cells[i].betta)+"\t"+s.FormatFloat("#0.000",StructPar.Cells[i].ELP)+"\t"+s.FormatFloat("#0.000000",StructPar.Cells[i].AL32)+"\t"+s.FormatFloat("#0.000000",StructPar.Cells[i].AkL);
		ParsedStrings->Add(F);

		StructPar.Cells[i].F0=StructPar.Sections[0].Frequency*1e6;
		StructPar.Cells[i].P0=StructPar.Sections[0].Power*1e6;
		StructPar.Cells[i].dF=0;
		StructPar.Cells[i].Drift=false;
		StructPar.Cells[i].First=false;
    }
    StructPar.Cells[0].First=true;

    ParsedStrings->Add("END");
    InputStrings->AddStrings(ParsedStrings);
    ParsedStrings->SaveToFile("PARSED.TXT");

    return Error;
}
//---------------------------------------------------------------------------
int TBeamSolver::ChangeCells(int N)
{
    TCell *iCells;
	iCells=new TCell[StructPar.NElements];

	for (int i=0;i<StructPar.NElements;i++)
		iCells[i]=StructPar.Cells[i];

	delete[] StructPar.Cells;

	int Nnew=StructPar.NElements<N?StructPar.NElements:N;
	int Nprev=StructPar.NElements;
	StructPar.NElements=N;

	StructPar.Cells=new TCell[StructPar.NElements];

    for (int i=0;i<Nnew;i++)
        StructPar.Cells[i]=iCells[i];

    delete[] iCells;

    return Nprev;
}
//---------------------------------------------------------------------------
void TBeamSolver::AppendCells(TCell& iCell,int N)
{
	int Nprev=ChangeCells(StructPar.NElements+N);
    int i=InputStrings->Count-1;
    InputStrings->Delete(i);

    i=Nprev;
	StructPar.Cells[i]=iCell;
	GetDimensions(StructPar.Cells[i]);

	for (int j=Nprev;j<StructPar.NElements;j++)
		StructPar.Cells[j]=StructPar.Cells[i];

    AnsiString F,F1,s;
	F1=F=s.FormatFloat("#0",iCell.Mode)+"\t"+s.FormatFloat("#0.000",StructPar.Cells[i].betta)+"\t"+s.FormatFloat("#0.000",StructPar.Cells[i].ELP)+"\t"+s.FormatFloat("#0.000000",StructPar.Cells[i].AL32)+"\t"+s.FormatFloat("#0.000000",StructPar.Cells[i].AkL);
	if (N==1)
		F="CELL "+F1;
    else
        F="CELLS "+s.FormatFloat("#0",N)+F1;

    InputStrings->Add(F);
    InputStrings->Add("END");

}
//---------------------------------------------------------------------------
void TBeamSolver::AddCells(int N)
{
	TCell pCell=StructPar.Cells[StructPar.NElements-1];
	AppendCells(pCell,N);
}
//---------------------------------------------------------------------------
TCell TBeamSolver::GetCell(int j)
{
    if (j<0)
        j=0;
	if (j>=StructPar.NElements)
		j=StructPar.NElements-1;

	return StructPar.Cells[j];
}
//---------------------------------------------------------------------------
TCell TBeamSolver::LastCell()
{
    return GetCell(StructPar.NElements-1);
}
//---------------------------------------------------------------------------
void TBeamSolver::ChangeInputCurrent(double Ib)
{
    BeamPar.Current=Ib;
}
//---------------------------------------------------------------------------
void TBeamSolver::SetBarsNumber(double Nbin)
{
	for (int i=0;i<Npoints;i++)
		Beam[i]->SetBarsNumber(Nbin);
}
//---------------------------------------------------------------------------
double *TBeamSolver::SmoothInterpolation(double *x,double *X,double *Y,int Nbase,int Nint,double p0,double *W)
{
    TSpline *Spline;
    double *y;
    //y=new double[Nint];

    Spline=new TSpline;
    Spline->MakeSmoothSpline(X,Y,Nbase,p0,W);
    y=Spline->Interpolate(x,Nint);

    delete Spline;

    return y;
}
//---------------------------------------------------------------------------
double *TBeamSolver::SplineInterpolation(double *x,double *X,double *Y,int Nbase,int Nint)
{
    TSpline *Spline;
    double *y;
    y=new double[Nint];

    Spline=new TSpline;
    Spline->MakeCubicSpline(X,Y,Nbase);
    y=Spline->Interpolate(x,Nint);

    delete Spline;

    return y;
}
//---------------------------------------------------------------------------
double *TBeamSolver::LinearInterpolation(double *x,double *X,double *Y,int Nbase,int Nint)
{
    TSpline *Spline;
    double *y;
	//y=new double[Nint];

    Spline=new TSpline;
    Spline->MakeLinearSpline(X,Y,Nbase);
    y=Spline->Interpolate(x,Nint);

    delete Spline;

    return y;
}
//---------------------------------------------------------------------------
int TBeamSolver::CreateGeometry()
{
    double theta=0;
    double *X_base,*B_base,*E_base,*Al_base,*A_base;
	double *X_int,*B_int,*E_int,*Al_int,*A_int;
	AnsiString S;

    bool Solenoid_success=false;

	int Njmp=0,k0=0;

	Npoints=0;
	for(int i=0;i<StructPar.NElements;i++){
		Npoints+=StructPar.Cells[i].Mesh;
		if (StructPar.Cells[i].First){
			Npoints++;
			Njmp++;
		}
	}

   //	Npoints=Ncells*Nmesh+Njmp;//add+1 for the last point;
	TSplineType Spl;

	X_base = new double[StructPar.NElements]; X_int=new double[Npoints];
	B_base = new double[StructPar.NElements]; B_int=new double[Npoints];
	E_base = new double[StructPar.NElements]; E_int=new double[Npoints];
	Al_base = new double[StructPar.NElements]; Al_int=new double[Npoints];

	delete[] Structure;

	double z=0,zm=0,D=0,x=0,lmb=0;

    Structure=new TStructure[Npoints];

    int k=0;
	for (int i=0;i<StructPar.NElements;i++){
        int Extra=0;

		if (i==StructPar.NElements-1)
            Extra=1;
		else if (StructPar.Cells[i+1].First)
            Extra=1;

		if (StructPar.Cells[i].First)
            z-=zm;

        double lmb=1;
		if (StructPar.Cells[i].Drift){
			D=StructPar.Cells[i].betta;
            bool isInput=false;
            for (int j=i;j<StructPar.NElements;j++){
				if (!StructPar.Cells[j].Drift){
					StructPar.Cells[i].betta=StructPar.Cells[j].betta;
					lmb=c/StructPar.Cells[j].F0;
                    isInput=true;
                    break;
                }
            }
            if (!isInput){
				for (int j=i;j>=0;j--){
					if (!StructPar.Cells[j].Drift){
						StructPar.Cells[i].betta=StructPar.Cells[j].betta;
						lmb=c/StructPar.Cells[j].F0;
                        isInput=true;
                        break;
                    }
                }
            }
            if (!isInput){
				StructPar.Cells[i].betta=1;
                lmb=1;
            }
        }else{
			lmb=c/StructPar.Cells[i].F0;
			theta=StructPar.Cells[i].Mode*pi/180;
			D=StructPar.Cells[i].betta*lmb*theta/(2*pi);
        }
        x+=D/2;
        X_base[i]=x/lmb;
		x+=D/2;
		B_base[i]=StructPar.Cells[i].betta;
		E_base[i]=StructPar.Cells[i].ELP;
		Al_base[i]=StructPar.Cells[i].AL32;
		//zm=D/Nmesh;
		zm=D/StructPar.Cells[i].Mesh;
		k0=k;
		Structure[k].dF=StructPar.Cells[i].dF;

		for (int j=0;j<StructPar.Cells[i].Mesh+Extra;j++){
            X_int[k]=z/lmb;
            Structure[k].ksi=z/lmb;
            Structure[k].lmb=lmb;
			Structure[k].P=StructPar.Cells[i].P0;
            Structure[k].dF=0;
			Structure[k].drift=StructPar.Cells[i].Drift;
			if (StructPar.Cells[i].Drift)
				Structure[k].Ra=StructPar.Cells[i].AkL/lmb;
            else
				Structure[k].Ra=StructPar.Cells[i].AkL;//*lmb;
            Structure[k].jump=false;
			Structure[k].CellNumber=i;
			//Structure[k].Dump=false;
			z+=zm;
            k++;
        }
		if (StructPar.Cells[i].First)
			Structure[k0].jump=true;
		for (int j=0; j < Ndump; j++) {
			if (BeamExport[j].NElement==i) {
				BeamExport[j].Nmesh=k0;
			}
		}
	  /*	if (StructPar.Cells[i].Dump){
			Structure[k0].Dump=true;
			Structure[k0].DumpParameters=StructPar.Cells[i].DumpParameters;
		  //	strcpy(Structure[k0].DumpParameters.File, Cells[i].DumpParameters.File);
		}    */
	}
   /*	if (StructPar.Cells[StructPar.NElements].Dump){
		Structure[k-1].Dump=true;  //was k0+Nmesh
		Structure[k-1].DumpParameters=StructPar.Cells[StructPar.NElements].DumpParameters; //was k0+Nmesh
	}    */

	//   int Njmp=0;
//  Structure[0].jump=true;

    double *Xo=NULL, *Bo=NULL, *Eo=NULL, *Ao=NULL;
	double *Xi=NULL, *Bi=NULL, *Ei=NULL, *Ai=NULL;
    int Ncls=0;
    int Npts=0;

    Njmp=0;
    int iJmp=0;

    bool EndOfBlock=false;

  /*    FILE *F;
    F=fopen("cells.log","w");*/

	for (int i=0;i<=StructPar.NElements;i++){
		if (i==StructPar.NElements)
			EndOfBlock=true;
		else if (StructPar.Cells[i].First && i!=0)
            EndOfBlock=true;
        else
            EndOfBlock=false;

        if (EndOfBlock/*Cells[i].First && i!=0 || i==Ncells*/){
			Ncls=i-Njmp;
			Npts=0;
			for (int j=Njmp;j<i;j++) {
				Npts+=StructPar.Cells[j].Mesh;
			}
			Npts++;
			//int Npts0=Ncls*Nmesh+1;

			/*if (i!=Ncells)
				Structure[i*Nmesh].jump=true;*/

            Xo=new double[Ncls];
            Bo=new double[Ncls];
            Eo=new double[Ncls];
            Ao=new double[Ncls];
            Xi=new double[Npts];

			for (int j=0;j<Ncls;j++){
                Xo[j]=X_base[Njmp+j];
                Bo[j]=B_base[Njmp+j];
                Eo[j]=E_base[Njmp+j];
                Ao[j]=Al_base[Njmp+j];
            }

			int pos=0;
			for (int j=0;j<Njmp;j++)
				pos+=StructPar.Cells[j].Mesh;

			for (int j=0;j<Npts;j++)
				Xi[j]=X_int[/*Njmp*Nmesh*/pos+iJmp+j];

            Spl=(Ncls<4)?LSPLINE:SplineType;

            if (Ncls==1)
                Spl=ZSPLINE;

            switch (Spl) {
                case ZSPLINE:{
                    Bi=new double[Npts];
                    Ei=new double[Npts];
                    Ai=new double[Npts];
                    for (int j=0;j<Npts;j++){
                        Bi[j]=Bo[0];
                        Ei[j]=Eo[0];
                        Ai[j]=Ao[0];
                    }
                    break;
                }
                case(LSPLINE):{
                    Bi=LinearInterpolation(Xi,Xo,Bo,Ncls,Npts);
                    Ei=LinearInterpolation(Xi,Xo,Eo,Ncls,Npts);
                    Ai=LinearInterpolation(Xi,Xo,Ao,Ncls,Npts);
                    break;
                }
                case(CSPLINE):{
                    Bi=SplineInterpolation(Xi,Xo,Bo,Ncls,Npts);
                    Ei=SplineInterpolation(Xi,Xo,Eo,Ncls,Npts);
                    Ai=SplineInterpolation(Xi,Xo,Ao,Ncls,Npts);
                    break;
                }
                case(SSPLINE):{
                    Bi=SmoothInterpolation(Xi,Xo,Bo,Ncls,Npts,Smooth);
                    Ei=SmoothInterpolation(Xi,Xo,Eo,Ncls,Npts,Smooth);
                    Ai=SmoothInterpolation(Xi,Xo,Ao,Ncls,Npts,Smooth);
                    break;
                }
            }

            for (int j=0;j<Npts;j++){
				B_int[/*Njmp*Nmesh*/pos+iJmp+j]=Bi[j];
				E_int[/*Njmp*Nmesh*/pos+iJmp+j]=Ei[j];
				Al_int[/*Njmp*Nmesh*/pos+iJmp+j]=Ai[j];
			}


        /*  for (int i=0;i<Npts;i++){
                fprintf(F,"%i %f %f\n",Njmp*Nmesh+i,Xi[i],Ei[i]);
            }     */


            delete[] Xo;
            delete[] Xi;
            delete[] Bo;
            delete[] Bi;
			delete[] Eo;
			delete[] Ei;
            delete[] Ao;
			delete[] Ai;

			Njmp=i;
			iJmp++;
		}

	}
	// fclose(F);

	if (StructPar.SolenoidPar.ImportType==IMPORT_1D) {
		int NSol=0;
		double *Xz=NULL;
		double *Bz=NULL;

		NSol=GetSolenoidPoints();
		if(NSol<1){
			StructPar.SolenoidPar.BField=0;
			StructPar.SolenoidPar.Length=0;
			StructPar.SolenoidPar.StartPos=0;
			StructPar.SolenoidPar.ImportType=NO_ELEMENT;
			/*Zmag=0;
			B0=0;
			Lmag=0;
			FSolenoid=false;  */
			S="The format of "+StructPar.SolenoidPar.File+" is wrong or too few points available";
			ShowError(S);
		} else {
			Xz=new double[NSol];
			Bz=new double[NSol];

			ReadSolenoid(NSol,Xz,Bz);
			if (NSol==1) {
				StructPar.SolenoidPar.StartPos=0;
				StructPar.SolenoidPar.Length=Structure[Npoints-1].ksi*Structure[Npoints-1].lmb;
				StructPar.SolenoidPar.BField=Bz[0];
				StructPar.SolenoidPar.ImportType=ANALYTIC_0D;
				/*Zmag=0;
				Lmag=Structure[Npoints-1].ksi*Structure[Npoints-1].lmb;
				B0=Bz[0];
				FSolenoid=false;*/
                delete[] Xz;
                delete[] Bz;
            } else{
                Xi=new double[Npoints];
				for (int i=0; i<Npoints; i++)
                    Xi[i]=Structure[i].ksi*Structure[i].lmb;

                Bi=LinearInterpolation(Xi,Xz,Bz,NSol,Npoints);
                delete[] Xz;
                delete[] Bz;
                delete[] Xi;
            }
        }
    }

	for (int i=0;i<Npoints;i++){
		Structure[i].Hext.z=0;
		Structure[i].Hext.r=0;
		Structure[i].Hext.th=0;
        //int s=0;
		double lmb=Structure[i].lmb;
		if (B_int[i]!=1)
			Structure[i].betta=B_int[i];
		else
            Structure[i].betta=MeVToVelocity(EnergyLimit);

        Structure[i].E=E_int[i];
        Structure[i].A=Structure[i].P>0?E_int[i]*sqrt(Structure[i].P)/We0:0;
/*      if (i==20)
            s=1;*/
		Structure[i].Rp=sqr(E_int[i])/2;;
        Structure[i].B=Structure[i].Rp/(2*We0);
		Structure[i].alpha=Al_int[i]/(lmb*sqrt(lmb));
		switch (StructPar.SolenoidPar.ImportType) {
			case IMPORT_1D:{
				Structure[i].Hext.z=Bi[i]*lmb*c/We0;
				//Bz_ext=Bi[i]*lmb*c/We0;
				break;
			}
			case ANALYTIC_0D:{
				if (Structure[i].ksi>=StructPar.SolenoidPar.StartPos/lmb && Structure[i].ksi<=(StructPar.SolenoidPar.StartPos+StructPar.SolenoidPar.Length)/lmb)
					//Structure[i].Bz_ext
					Structure[i].Hext.z=StructPar.SolenoidPar.BField*lmb*c/We0;
				break;
			}
			default: { }
		}
	}

   /*	Structure[0].Br_ext=0;
	Structure[Npoints-1].Br_ext=0;  */
	for (int i=1;i<Npoints-1;i++){
		switch (StructPar.SolenoidPar.ImportType) {
			case IMPORT_1D:{}
			case ANALYTIC_0D:{
				Structure[i].Hext.r=-0.5*(Structure[i+1].Hext.z-Structure[i-1].Hext.z)/(Structure[i+1].ksi-Structure[i-1].ksi);
				//Structure[i].Br_ext=-0.5*(Structure[i+1].Bz_ext-Structure[i-1].Bz_ext)/(Structure[i+1].ksi-Structure[i-1].ksi);
				break;
			}
			default: {	}
		}

	}

   /*	if (!BeamPar.Magnetized) {
    	Structure[0].B_ext=0;
	}       */

	if (StructPar.SolenoidPar.ImportType==IMPORT_1D)
		delete[] Bi;

    delete[] X_base; delete[] X_int;
    delete[] B_base; delete[] B_int;
    delete[] E_base; delete[] E_int;
    delete[] Al_base; delete[] Al_int;

    return ERR_NO;
}
//---------------------------------------------------------------------------
TError TBeamSolver::CreateBeam()
{
    double sx=0,sy=0,r=0;
	double b0=0,db=0;

	AnsiString S;

	//Npoints=Ncells*Nmesh;

	//Np=BeamPar.NParticles;

	//OLD CODE
   /* if (BeamType!=RANDOM){
	    if (NpFromFile)
		    Np=Beam[0]->CountCSTParticles(BeamFile);
        if(Np<0) {
            return ERR_CURRENT;
		}
    }

	if (NpEnergy != 0) {
	    if (NpEnergy != Np) {
            return ERR_NUMBPARTICLE;
		}
	} */

   for (int i=0;i<Np_beam;i++){
        delete Beam[i];
    }
    delete[] Beam;

    Beam=new TBeam*[Npoints];
	for (int i=0;i<Npoints;i++){
		Beam[i]=new TBeam(BeamPar.NParticles);
	  //  Beam[i]->SetBarsNumber(Nbars);
		//Beam[i]->SetKernel(Kernel);
		Beam[i]->lmb=Structure[i].lmb;
		Beam[i]->SetInputCurrent(BeamPar.Current);
		Beam[i]->Reverse=StructPar.Reverse;
		//Beam[i]->Cmag=c*Cmag/(Structure[i].lmb*We0); //Cmag = c*B*lmb/Wo * (1/lmb^2 from r normalization)
		for (int j=0;j<BeamPar.NParticles;j++){
			Beam[i]->Particle[j].lost=LIVE;
			Beam[i]->Particle[j].beta0=0;
			Beam[i]->Particle[j].beta.z=0;
			Beam[i]->Particle[j].beta.r=0;
			Beam[i]->Particle[j].beta.th=0;
			//Beam[i]->Particle[j].Br=0;
			Beam[i]->Particle[j].phi=0;
		   //	Beam[i]->Particle[j].Bth=0;
			Beam[i]->Particle[j].r=0;
			//Beam[i]->Particle[j].Cmag=0;
			Beam[i]->Particle[j].Th=0;
		}
	}
	Beam[0]->SetCurrent(BeamPar.Current);
	Np_beam=Npoints;

	for (int i=0;i<BeamPar.NParticles;i++)
        Beam[0]->Particle[i].z=Structure[0].ksi*Structure[0].lmb;

	//Create Distributions:

	if (!IsFullFileKeyWord(BeamPar.RBeamType)){
		switch (BeamPar.ZBeamType){
			case NORM_2D: {
				Beam[0]->GenerateEnergy(BeamPar.WNorm);
				Beam[0]->GeneratePhase(BeamPar.ZNorm);
				break;
			}
			case FILE_1D: {
				Beam[0]->GeneratePhase(BeamPar.ZNorm);
			}
			case FILE_2D: {
				if (!Beam[0]->ImportEnergy(&BeamPar))
					return ERR_BEAM;
				break;
			}
			default: {
				S="Unexpected Distribution Type";
				ShowError(S);
				return ERR_BEAM;
			}
		}
	}

	switch (BeamPar.RBeamType) {
		case CST_PID:{
			Beam[0]->GeneratePhase(BeamPar.ZNorm);
		}
		case CST_PIT:{
			if (!Beam[0]->BeamFromCST(&BeamPar))
				return ERR_BEAM;
			break;
		}
		case TWISS_2D:{}
		case TWISS_4D:{
			Beam[0]->BeamFromTwiss(&BeamPar);
			break;;
		}
		case SPH_2D:{
			Beam[0]->BeamFromSphere(&BeamPar);
			break;
		}
		case ELL_2D:{
			Beam[0]->BeamFromEllipse(&BeamPar);
			break;
		}
		case FILE_2D: {
			TGauss A;
			A.mean=0;
			A.limit=pi;
			A.sigma=100*pi;
			Beam[0]->GenerateAzimuth(A);
		}
		case TWO_FILES_2D: { }
		case FILE_4D:{
			if (!Beam[0]->BeamFromFile(&BeamPar))
				return ERR_BEAM;
			break;
		}
		default: {
			S="Unexpected Distribution Type";
			ShowError(S);
			return ERR_BEAM;
		}
	}

  //	if (BeamPar.Magnetized) {
	  /*	for (int j=0;j<BeamPar.NParticles;j++){
			double gamma=VelocityToEnergy(Beam[0]->Particle[j].beta);
			double bth=Beam[0]->Particle[j].Bth;
			double r=Beam[0]->Particle[j].r;
			double Bz=Structure[0].Hext.z;
			double Cmag=2*r*gamma*bth+sqr(r)*Bz;
			//double Cmag=sqr(r)*Bz;
			for (int i=0;i<Npoints;i++)
				Beam[i]->Particle[j].Cmag=Cmag;
		}      */
   //	}

   //OLD CODE
/*
	if (NpEnergy == 0) {
        if (W_Eq) {
            Beam[0]->MakeEquiprobableDistribution(W0,dW,BETTA_PAR);
        } else {
            Beam[0]->MakeGaussDistribution(W0,dW,BETTA_PAR);
        }
	} else {
		LoadEnergyFromFile(EnergyFile,NpEnergy);
	}

    for (int i=0;i<Np;i++) {
        Beam[0]->Particle[i].betta=MeVToVelocity(Beam[0]->Particle[i].betta);
    }

    if (Phi_Eq) {
        Beam[0]->MakeEquiprobableDistribution(HellwegTypes::DegToRad(Phi0)-Structure[0].dF,HellwegTypes::DegToRad(dPhi),PHI_PAR);
    } else {
        Beam[0]->MakeGaussDistribution(HellwegTypes::DegToRad(Phi0)-Structure[0].dF,HellwegTypes::DegToRad(dPhi),PHI_PAR);
    }

	if (BeamType==RANDOM) {
		Beam[0]->MakeGaussEmittance(AlphaCS,BettaCS,EmittanceCS);
    } else {
		Beam[0]->ReadCSTEmittance(BeamFile,Np);
	}

   //	if (BeamType!=CST_Y) {  //not used yet
        Beam[0]->MakeEquiprobableDistribution(pi,pi,TH_PAR);
   //	}
    Beam[0]->MakeEquiprobableDistribution(0,0,BTH_PAR);

    for (int i=0;i<Npoints;i++){
        for (int j=0;j<Np;j++) {
            Beam[i]->Particle[j].x0=Beam[0]->Particle[j].x;
        }
	}    */

 /* for (int i=0;i<Np;i++){
        Beam[0]->Particle[i].x=0;//-0.001+0.002*i/(Np-1);
        //Beam[0]->Particle[i].phi=0;
        Beam[0]->Particle[i].Bx=0;
        Beam[0]->Particle[i].Th=0;
        Beam[0]->Particle[i].Bth=0;

      //    Beam[0]->Particle[i].phi=DegToRad(-90+i);
       //   Beam[0]->Particle[i].betta=MeVToVelocity(0.05);
    }           */
    return ERR_NO;
}
//---------------------------------------------------------------------------
//MOVED TO BEAM.CPP
/*bool TBeamSolver::LoadEnergyFromFile(AnsiString &F, int NpEnergy)
{
	std::ifstream fs(F.c_str());
	float enrg=0;
	int i=0, np=0;
	bool Success=false;
	AnsiString S,L;

	while (!fs.eof()){
		L=GetLine(fs);

		if (NumWords(L)==2){       //Check if only two numbers in the line
			try {                  //Check if the data is really a number
				S=ReadWord(L,1);
				np=S.ToInt();
				S=ReadWord(L,2);
				enrg=S.ToDouble();
			}
			catch (...){
				continue;          //Skip incorrect line
			}
			if (i==NpEnergy){     //if there is more data than expected
				i++;
				break;
			}
			Beam[0]->Particle[i].betta=enrg;
			i++;
		}
	}

	fs.close();

	Success=(i==NpEnergy);
	return Success;
}       */
//---------------------------------------------------------------------------
int TBeamSolver::GetSolenoidPoints()
{
	return NumPointsInFile(StructPar.SolenoidPar.File,2);
}
//---------------------------------------------------------------------------
bool TBeamSolver::ReadSolenoid(int Nz,double *Z,double* B)
{
	std::ifstream fs(StructPar.SolenoidPar.File.c_str());
	float z=0,Bz=0;
	int i=0;
	bool Success=false;
	AnsiString S,L;

	while (!fs.eof()){
		L=GetLine(fs);

		if (NumWords(L)==2){       //Check if only two numbers in the line
			try {                  //Check if the data is really a number
				S=ReadWord(L,1);
				z=S.ToDouble()/100; //[cm]
				S=ReadWord(L,2);
				Bz=S.ToDouble()/10000;  //[Gs]
			}
			catch (...){
				continue;          //Skip incorrect line
			}
			if (i==Nz){  //if there is more data than expected
				i++;
				break;
			}
			Z[i]=z;
			B[i]=Bz;
			i++;
		}
	}

	fs.close();

	Success=(i==Nz);
	return Success;
}
//---------------------------------------------------------------------------
TEllipse TBeamSolver::GetEllipse(int Nknot,TBeamParameter P)
{
	//return Beam[Nknot]->GetEllipseDirect(P);
	return Beam[Nknot]->GetEllipse(P);
}
//---------------------------------------------------------------------------
TTwiss TBeamSolver::GetTwiss(int Nknot,TBeamParameter P)
{
	//return Beam[Nknot]->GetTwissDirect(P);
	return Beam[Nknot]->GetTwiss(P);
}
//---------------------------------------------------------------------------
TGauss TBeamSolver::GetEnergyStats(int Nknot, TDeviation D)
{
	return Beam[Nknot]->GetEnergyDistribution(D);
}
//---------------------------------------------------------------------------
TGauss TBeamSolver::GetPhaseStats(int Nknot, TDeviation D)
{
	return Beam[Nknot]->GetPhaseDistribution(D);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeamSolver::GetSpectrum(int Nknot,TBeamParameter P,bool Smooth)
{
	return Beam[Nknot]->GetSpectrumBar(P,Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeamSolver::GetEnergySpectrum(int Nknot,bool Smooth)
{
	return Beam[Nknot]->GetEnergySpectrum(Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeamSolver::GetPhaseSpectrum(int Nknot,bool Smooth)
{
	return Beam[Nknot]->GetPhaseSpectrum(Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeamSolver::GetRSpectrum(int Nknot,bool Smooth)
{
	return Beam[Nknot]->GetRSpectrum(Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeamSolver::GetXSpectrum(int Nknot,bool Smooth)
{
	return Beam[Nknot]->GetXSpectrum(Smooth);
}
//---------------------------------------------------------------------------
TSpectrumBar *TBeamSolver::GetYSpectrum(int Nknot,bool Smooth)
{
	return Beam[Nknot]->GetYSpectrum(Smooth);
}
//---------------------------------------------------------------------------
double TBeamSolver::GetSectionFrequency(int Nsec)
{
	return StructPar.Sections[Nsec].Frequency;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetSectionPower(int Nsec)
{
	return StructPar.Sections[Nsec].Power;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetSectionWavelength(int Nsec)
{
	return c/StructPar.Sections[Nsec].Frequency;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetFrequency(int Ni)
{
	return c/Structure[Ni].lmb;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetPower(int Ni)
{
	return Structure[Ni].P;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetWavelength(int Ni)
{
	return Structure[Ni].lmb;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetAperture(int Ni)
{
	return Structure[Ni].Ra*Structure[Ni].lmb;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetMaxEnergy(int Ni)
{
	return Beam[Ni]->GetMaxEnergy();
}
//---------------------------------------------------------------------------
double TBeamSolver::GetMaxDivergence(int Ni)
{
	return Beam[Ni]->GetMaxDivergence();
}
//---------------------------------------------------------------------------
double TBeamSolver::GetMaxPhase(int Ni)
{
	return Beam[Ni]->GetMaxPhase();
}
//---------------------------------------------------------------------------
double TBeamSolver::GetMinPhase(int Ni)
{
	return Beam[Ni]->GetMinPhase();
}
//---------------------------------------------------------------------------
double TBeamSolver::GetCurrent(int Ni)
{
	return Beam[Ni]->GetCurrent();
}
//---------------------------------------------------------------------------
double TBeamSolver::GetBeamRadius(int Ni,TBeamParameter P)
{
	return Beam[Ni]->GetRadius(P);
}
//---------------------------------------------------------------------------
TLoss TBeamSolver::GetLossType(int Nknot,int Np)
{
	return Beam[Nknot]->Particle[Np].lost;
}
//---------------------------------------------------------------------------
int TBeamSolver::GetLivingNumber(int Ni)
{
	return Beam[Ni]->GetLivingNumber();
}
//---------------------------------------------------------------------------
TGraph *TBeamSolver::GetTrace(int Np,TBeamParameter P1,TBeamParameter P2)
{
	TGraph *G;
	G=new TGraph[Npoints];
	bool live=false;

	for (int i = 0; i < Npoints; i++) {
		live=Beam[i]->GetParameter(Np,LIVE_PAR)==1;
		if (live) {
			G[i].x=Beam[i]->GetParameter(Np,P1);
			G[i].y=Beam[i]->GetParameter(Np,P2);
		}
		G[i].draw=live;
	}

	return G;
}
//---------------------------------------------------------------------------
TGraph *TBeamSolver::GetSpace(int Nknot,TBeamParameter P1,TBeamParameter P2)
{
	TGraph *G;
	G=new TGraph[BeamPar.NParticles];
	bool live=false;

	for (int i = 0; i < BeamPar.NParticles; i++) {
		live=Beam[Nknot]->GetParameter(i,LIVE_PAR)==1;
		if (live) {
			G[i].x=Beam[Nknot]->GetParameter(i,P1);
			G[i].y=Beam[Nknot]->GetParameter(i,P2);
		}
		G[i].draw=live;
	}

	return G;
}
/*
//OBSOLETE
//---------------------------------------------------------------------------
TSpectrumBar *TBeamSolver::GetEnergySpectrum(int Nknot,double& Wav,double& dW)
{
    TSpectrumBar *Spectrum;
    Spectrum=GetEnergySpectrum(Nknot,false,Wav,dW);
    return Spectrum;
}

//---------------------------------------------------------------------------
TSpectrumBar *TBeamSolver::GetPhaseSpectrum(int Nknot,double& Fav,double& dF)
{
	TSpectrumBar *Spectrum;
	Spectrum=GetPhaseSpectrum(Nknot,false,Fav,dF);
	return Spectrum;
}
*/
//---------------------------------------------------------------------------
double *TBeamSolver::GetBeamParameters(int Nknot,TBeamParameter P)
{
	double *X;
	X=new double[BeamPar.NParticles];

	for (int i= 0; i < BeamPar.NParticles; i++) {
		X[i]=Beam[Nknot]->GetParameter(i,P);
	}

	return X;

}
//---------------------------------------------------------------------------
double TBeamSolver::GetStructureParameter(int Nknot, TStructureParameter P)
{
	double x=0;
	TTwiss T;

	switch (P) {
		case (KSI_PAR):{
			x=Structure[Nknot].ksi;
			break;
		}
		case (Z_PAR):{
			x=Structure[Nknot].ksi*Structure[Nknot].lmb;
			break;
		}
		case (A_PAR):{
			x=Structure[Nknot].A;
			break;
		}
		case (RP_PAR):{
			x=Structure[Nknot].Rp;
			break;
		}
		case (E0_PAR):{
			x=Structure[Nknot].A*We0/Structure[Nknot].lmb;
			break;
		}
		case (EREAL_PAR):{
			double P=Structure[Nknot].P>0?Structure[Nknot].P:0;
			x=sqrt(2*Structure[Nknot].Rp)*sqrt(P)/Structure[Nknot].lmb;
			break;
		}
		case (PRF_PAR):{
			double E=sqrt(2*Structure[Nknot].Rp);
			x=E!=0?sqr(Structure[Nknot].A*We0/E):0;
			break;
		}
		case (PBEAM_PAR):{
			x=Beam[Nknot]->GetAverageEnergy()*Beam[Nknot]->GetCurrent();
			break;
		}
		case (ALPHA_PAR):{
			x=Structure[Nknot].alpha;
			break;
		}
		case (SBETA_PAR):{
			x=Structure[Nknot].betta;
			break;
		}
		case (BBETA_PAR):{
			x=MeVToVelocity(Beam[Nknot]->GetAverageEnergy());
			break;
		}
		case (WAV_PAR):{
			x=Beam[Nknot]->GetAverageEnergy();
			break;
		}
		case (WMAX_PAR):{
			x=Beam[Nknot]->GetMaxEnergy();
			break;
		}
		case (RA_PAR):{
			x=Structure[Nknot].Ra*Structure[Nknot].lmb;
			break;
		}
		case (RB_PAR):{
			x=Beam[Nknot]->GetRadius();
			break;
		}
		case (B_EXT_PAR):{
			x=Structure[Nknot].Hext.z;
			break;
		}
		case (NUM_PAR):{
			x=Structure[Nknot].CellNumber;
			break;
		}
		case (ER_PAR):{
			T=Beam[Nknot]->GetTwiss(R_PAR,false);
			x=T.epsilon;
			break;
		}
		case (EX_PAR):{
			T=Beam[Nknot]->GetTwiss(X_PAR,false);
			x=T.epsilon;
			break;
		}
		case (EY_PAR):{
			T=Beam[Nknot]->GetTwiss(Y_PAR,false);
			x=T.epsilon;
			break;
		}
		case (ENR_PAR):{
			T=Beam[Nknot]->GetTwiss(R_PAR,true);
			x=T.epsilon;
			break;
		}
		case (ENX_PAR):{
			T=Beam[Nknot]->GetTwiss(X_PAR,true);
			x=T.epsilon;
			break;
		}
		case (ENY_PAR):{
			T=Beam[Nknot]->GetTwiss(Y_PAR,true);
			x=T.epsilon;
			break;
		}
		case (E4D_PAR):{
			x=Beam[Nknot]->Get4DEmittance(false);
			break;
		}
		case (E4DN_PAR):{
			x=Beam[Nknot]->Get4DEmittance(true);
			break;
		}
		case (ET_PAR):{
			T=Beam[Nknot]->GetTwiss(TH_PAR,false);
			x=T.epsilon;
			break;
		}
		case (ENT_PAR):{
			T=Beam[Nknot]->GetTwiss(TH_PAR,true);
			x=T.epsilon;
			break;
		}
                default: {
                        throw runtime_error("GetStructureParameter error: Unhandled TStructureParameter value");
                }
	}

	return x;
}
//---------------------------------------------------------------------------
double *TBeamSolver::GetStructureParameters(TStructureParameter P)
{
	double *X;
	X=new double [Npoints];

	for (int i=0;i<Npoints;i++){
		X[i]=GetStructureParameter(i,P);
	}

	return X;
}
//---------------------------------------------------------------------------
/*double TBeamSolver::GetKernel()
{
	return Beam[0]->h;
} */

//---------------------------------------------------------------------------
void TBeamSolver::Abort()
{
	Stop=true;
}
//---------------------------------------------------------------------------
double TBeamSolver::GetEigenFactor(double x, double y, double z,double a, double b, double c)
{
	double s=0,s1=0,s2=0,s3=0;
	double A=0, B=0, C=0, D=0;
	double q=0, p=0, Q=0;
	double rho=0, cph=0, phi=0;

	double xx=sqr(x/a),yy=sqr(y/b),zz=sqr(z/c),bb=sqr(a/b),cc=sqr(a/c);

	A=bb*cc;
	//B=(bb*cc+bb+cc)-(xx*bb*cc+yy*cc+zz*bb);
	B=A*(1-xx)+bb*(1-zz)+cc*(1-zz);
	//C=1-yy-zz+bb*(1-xx-zz)+cc*(1-xx-yy);
	//C=(1-xx)*(bb+cc)-yy*(1+cc)-zz*(1+bb);
	C=1+bb+cc-xx*(bb+cc)-yy*(1+cc)-zz*(1+bb);
	D=1-(xx+yy+zz);

	p=C/A-sqr(B/A)/3;
	q=(2*cub(B)-9*A*B*C+27*sqr(A)*D)/(27*cub(A));
	Q=cub(p/3)+sqr(q/2);

	if (Q > 0) {
		s=cubrt(sqrt(Q)-q/2)-cubrt(sqrt(Q)+q/2);
	} else if (Q == 0) {
		s1=2*cubrt(q/2);
		s2=-2*cubrt(q/2);
		s=s2>s1?s2:s1;
	} else {
		rho=sqrt(-p/3);//sqrt(-cub(p)/27);
		//cph=-q/(2*rho);
		cph=-q*pow(-3/p,1.5)/2;
		phi=acos(cph);
		//double r=sqrt(-p/3);
		s1=2*rho*cos(phi/3);
		s2=2*rho*cos(phi/3+2*pi/3);
		s3=2*rho*cos(phi/3+4*pi/3);
		s=s2>s1?s2:s1;
		s=s3>s?s3:s;
		s=/*sqr(a)*/(s-B/(3*A));
    }

	return s;
}
//---------------------------------------------------------------------------
double TBeamSolver::FormFactor(double ryrx, double rxrz, TBeamParameter P, double s)
{
	int kx=0, ky=0, kz=0, Nx=Nryrx, Ny=Nrxrz, Nz=Nlmb, Ndim=8;
	double *M=NULL;
	double *X=(double *)RYRX;
	double *Y=(double *)RXRZ;
	double *Z=(double *)LMB;
	double F=0;
	double x=ryrx, y=rxrz, z=s;

	//M=new double** [Ny];


	if (x<=X[0]){
		x=X[0];
		kx=0;
	}else if (x>=X[Nx-1]){
		x=X[Nx-1];
		kx=Nx-2;
	}else {
		for (int i = 1; i < Nx; i++) {
			if (x<X[i]) {
				kx=i-1;
				break;
			}
		}
	}

	if (y<=Y[0]){
		y=Y[0];
		ky=0;
	}else if (y>=Y[Ny-1]){
		y=Y[Ny-1];
		ky=Ny-2;
	}else {
		for (int i = 1; i < Ny; i++) {
			if (y<Y[i]) {
				ky=i-1;
				break;
			}
		}
	}

	if (z<=Z[0]){
		z=Z[0];
		kz=0;
	}else if (z>=Z[Nz-1]){
		z=Z[Nz-1];
		kz=Nz-2;
		return 0;
	}else {
		for (int i = 1; i < Nz; i++) {
			if (z<Z[i]) {
				kz=i-1;
				break;
			}
		}
	}

	M=new double [Ndim]; //4*kz + 2*ky + kx

	for (int i=0; i<=1; i++){
		for (int j=0; j<=1; j++){
			for (int k=0; k<=1; k++){
				switch (P) {
					case X_PAR:{
						M[4*k+2*j+i]=MX[kz+k][ky+j][kx+i];
						break;
					}
					case Y_PAR:{
						M[4*k+2*j+i]=MY[kz+k][ky+j][kx+i];
						break;
					}
					case Z0_PAR:{
						M[4*k+2*j+i]=MZ[kz+k][ky+j][kx+i];
						//M[4*k+2*j+i]=MZ[kz+k][ky+j][kx+i];
						break;
					}
					default:{
						M[4*k+2*j+i]=0;
					}
				}
			//	if (M[4*k+2*j+i]>0)
					M[4*k+2*j+i]=log(M[4*k+2*j+i]);
			 //	else
				 //	ShowMessage("Pidor!");
			}
		}
	}

	//M[0]=M[kz][ky][kx];
	//M[1=M[kz][ky][kx+1];
	//M[2]=M[kz][ky+1][kx];
	//M[3]=M[kz][ky+1][kx+1];
	//M[4]=M[kz+1][ky][kx];
	//M[5]=M[kz+1][ky][kx+1];
	//M[6]=M[kz+1][ky+1][kx];
	//M[7]=M[kz+1][ky+1][kx+1];

	/*F=(M[ky][kx]*(X[kx+1]-x)*(Y[ky+1]-y)+M[ky][kx+1]*(x-X[kx])*(Y[ky+1]-y)+
	M[ky+1][kx]*(X[kx+1]-x)*(y-Y[ky])+M[ky+1][kx+1]*(x-X[kx])*(y-Y[ky]))/((X[kx+1]-X[kx])*(Y[ky+1]-Y[ky]));*/

	F=M[0]*(X[kx+1]-x)*(Y[ky+1]-y)*(Z[kz+1]-z)+
	M[1]*(x-X[kx])*(Y[ky+1]-y)*(Z[kz+1]-z)+
	M[2]*(X[kx+1]-x)*(y-Y[ky])*(Z[kz+1]-z)+
	M[3]*(x-X[kx])*(y-Y[ky])*(Z[kz+1]-z)+
	M[4]*(X[kx+1]-x)*(Y[ky+1]-y)*(z-Z[kz])+
	M[5]*(x-X[kx])*(Y[ky+1]-y)*(z-Z[kz])+
	M[6]*(X[kx+1]-x)*(y-Y[ky])*(z-Z[kz])+
	M[7]*(x-X[kx])*(y-Y[ky])*(z-Z[kz]);

	F/=(X[kx+1]-X[kx])*(Y[ky+1]-Y[ky])*(Z[kz+1]-Z[kz]);

	delete[] M;

	return exp(F);
}
//---------------------------------------------------------------------------
void TBeamSolver::Integrate(int Si, int Sj)
{
	double gamma0=1,Mr=0,Nr=0,/*phic=0,*/Qbunch=0,Rho=0,lmb=0,Icur=0;
	double Mx=0,My=0,Mz=0,Mxx=0,Myy=0,Mzz=0,ix=0,iy=0,p=0,M=0;
	int component=0;
	TParticle *Particle=Beam[Si]->Particle;

	//phic=Beam[Si]->iGetAveragePhase(Par[Sj],K[Sj]);
	Par[Sj].SumSin=0;
	Par[Sj].SumCos=0;
	Par[Sj].SumSin=Beam[Si]->SinSum(Par[Sj],K[Sj]);
    Par[Sj].SumCos=Beam[Si]->CosSum(Par[Sj],K[Sj]);

	gamma0=Beam[Si]->iGetAverageEnergy(Par[Sj],K[Sj]);
	Par[Sj].gamma=gamma0;

	double x0=0,y0=0,z0=0,rx=0,ry=0,rz=0/*,rb=0*/,V=0,Mcore=1,Ncore=0;
	double Nrms=BeamPar.SpaceCharge.Nrms;

	Par[Sj].Eq=new TField[BeamPar.NParticles];

	//FILE *Fout=fopen("spch.log","a");
	//fprintf(Fout,"x y z lambda Mx My Mz\n");
	//fprintf(Fout,"x Ex\n");
	//fprintf(Fout,"%f %f %f %f %f %f %f %f %f\n",n,Ncore,Mcore,1e9*Qbunch,V,Rho,Mz,Rho*Mz,Rho*Mz*rz);

	for (int i=0;i<BeamPar.NParticles;i++){
		Par[Sj].Eq[i].z=0;
		Par[Sj].Eq[i].r=0;
		Par[Sj].Eq[i].th=0;
	}

	Ncore=0;
	switch (BeamPar.SpaceCharge.Type) {   //SPACE CHARGE
		case SPCH_LPST: {}
		case SPCH_ELL: {
			TGauss Gx,Gy,Gz;
			Gx=Beam[Si]->iGetBeamRadius(Par[Sj],K[Sj],X_PAR);
			Gy=Beam[Si]->iGetBeamRadius(Par[Sj],K[Sj],Y_PAR);
			Gz=Beam[Si]->iGetBeamLength(Par[Sj],K[Sj]);//BeamPar.SpaceCharge.NSlices);

			x0=Gx.mean;
			y0=Gy.mean;
			z0=Gz.mean;

			rx=Nrms*Gx.sigma;
			ry=Nrms*Gy.sigma;
			rz=Nrms*Gz.sigma;

			lmb=Beam[Si]->lmb;
			Icur=Beam[Si]->GetCurrent();
			Qbunch=Icur*lmb/c;
			V=mod(4*pi*rx*ry*rz/3);
			Rho=Qbunch/V;
			//Rho*=2*pi/eps0;
			Rho/=eps0;

			if (V!=0) {
				if (BeamPar.SpaceCharge.Type==SPCH_LPST) {  //Lapostolle
					p=gamma0*rz/sqrt(rx*ry);
					M=FormFactorLpst(p);
				} else {     //Elliptic
					ix=ry/rx;
					iy=rx/rz;
					Mx=FormFactor(ix,iy,X_PAR);
					My=FormFactor(ix,iy,Y_PAR);
					Mz=FormFactor(ix,iy,Z0_PAR);
				}
				for (int i=0;i<BeamPar.NParticles;i++){
					if (Particle[i].lost==LIVE){
						double x=0,y=0,z=0,r=0,th=0,phi=0,gamma=1,g2=1;
						double Ex=0,Ey=0,Ez=0;
						double r3=0,s=0;

						phi=Particle[i].phi+K[Sj][i].phi*Par[Sj].h;
						r=(Particle[i].r+K[Sj][i].r*Par[Sj].h)*lmb;
						th=Particle[i].Th+K[Sj][i].th*Par[Sj].h;

						x=r*cos(th)-x0;
						y=r*sin(th)-y0;
						z=phi*lmb/(2*pi)-z0;
						r3=sqr(x/rx)+sqr(y/ry)+sqr(z/rz);
						gamma=VelocityToEnergy(Particle[i].beta.z+K[Sj][i].beta.z*Par[Sj].h); //change to beta
						g2=1/sqr(gamma);

						if  (BeamPar.SpaceCharge.Type==SPCH_LPST) { //Lapostolle
							Ex=Rho*(1-M)*x*ry/(rx+ry);
							Ey=Rho*(1-M)*x*rx/(rx+ry);
							Ez=Rho*M*z;
						}  else {//Elliptic
							if (r3<1) {
								s=0;
								Ex=Rho*Mx*x;
								Ey=Rho*My*y;
								Ez=Rho*Mz*z;
								Ncore++;
							} else {
								s=GetEigenFactor(x,y,z,rx,ry,rz);
								Mxx=FormFactor(ix,iy,X_PAR,s);
								Myy=FormFactor(ix,iy,Y_PAR,s);
								Mzz=FormFactor(ix,iy,Z0_PAR,s);

								Ex=Rho*Mxx*x/2;
								Ey=Rho*Myy*y/2;
								Ez=Rho*Mzz*z/2;
							}
						}

						Ex*=(g2*lmb/We0);
						Ey*=(g2*lmb/We0);
						Ez*=(lmb/We0);

						Par[Sj].Eq[i].z=Ez;
						Par[Sj].Eq[i].r=Ex*cos(th)+Ey*sin(th);
						Par[Sj].Eq[i].th=-Ex*sin(th)+Ey*cos(th);
					}
				}
				if (BeamPar.SpaceCharge.Type==SPCH_ELL) {// elliptic
					Mcore=Ncore/Nliv;
					for (int i=0;i<BeamPar.NParticles;i++){
						Par[Sj].Eq[i].z*=Mcore;
						Par[Sj].Eq[i].r*=Mcore;
						Par[Sj].Eq[i].th*=Mcore;
					}
				}
			}
			break;
		}
		case SPCH_GW: {
		  /*	for (int i=0;i<BeamPar.NParticles;i++){
				Par[Sj].Eq[i].z=kFc*Icur*lmb*GaussIntegration(r,z,Rb,Lb,3);  // E;  (YuE) expression
				Par[Sj].Eq[i].z*=(lmb/We0);                     // A
				Par[Sj].Eq[i].r=kFc*Icur*lmb*GaussIntegration(r,z,Rb,Lb,1);  // E;  (YuE) expression
				Par[Sj].Eq[i].r*=(lmb/We0);                     // A
			}     */
			break;
		}
		case SPCH_NO:{}
		default: { };
	}
	//fclose(Fout);

	Beam[Si]->Integrate(Par[Sj],K,Sj);
	delete[] Par[Sj].Eq;
	//delete[] Par[Sj].Aqr;
}
//---------------------------------------------------------------------------
 double TBeamSolver::GaussIntegration(double r,double z,double Rb,double Lb,int component)
{
  //MOVED CONSTANTS TO CONST.H!

	double Rb2,Lb2,d,d2,ksi,s,t,func,GInt;
	int i;

	Rb2=sqr(Rb);
	Lb2=sqr(Lb);
	d=pow(Rb2*Lb,1/3);
	d2=sqr(d);
	GInt=0;
	for (int i=0;i<11;i++) {
	    ksi=.5*(psi12[i]+1);
	    s=d2*(1/ksi-1);
	    t=sqr(r)/(Rb2+s)+sqr(z)/(Lb2+s);
		if (t <= 5) {
//           func=sqrt(ksi/((Lb2/d2-1)*ksi+1))/abs(((Rb2/d2-1)*ksi+1));
           func=sqrt(ksi/((Lb2/d2-1)*ksi+1))/((Rb2/d2-1)*ksi+1);
		   if (component < 3) {
//		        GInt +=.5*r*w12[i]*func/abs(((Rb2/d2-1)*ksi+1));
		        GInt +=.5*r*w12[i]*func/((Rb2/d2-1)*ksi+1);
		   }
		   if (component == 3) {
//		        GInt +=.5*z*w12[i]*func/abs(((Lb2/d2-1)*ksi+1));
		        GInt +=.5*z*w12[i]*func/((Lb2/d2-1)*ksi+1);
		   }
		}
	}
	return GInt;
}
//---------------------------------------------------------------------------
void TBeamSolver::CountLiving(int Si)
{
    Nliv=Beam[Si]->GetLivingNumber();
    if (Nliv==0){
      /*    FILE *F;
        F=fopen("beam.log","w");
        for (int i=Si;i<Npoints;i++){
            for (int j=0;j<Np;j++)
                fprintf(F,"%i ",Beam[i]->Particle[j].lost);
            fprintf(F,"\n");
        }
        fclose(F);   */
		#ifndef RSLINAC
		AnsiString S="Beam Lost!";
		ShowError(S);
        #endif
        Stop=true;
        return;
    }
}
//---------------------------------------------------------------------------
void TBeamSolver::DumpHeader(ofstream &fo,int Sn,int jmin,int jmax)
{
	AnsiString s;
	fo<<"List of ";

	int Si=BeamExport[Sn].Nmesh;

	if (jmin==0 && jmax==BeamPar.NParticles)
		fo<<"ALL ";
	if (BeamExport[Sn].LiveOnly)
		fo<<"LIVE ";

	fo<<"particles ";

	if (!(jmin==0 && jmax==BeamPar.NParticles)){
		fo<<" from #";
		fo<<jmin+1;
		fo<<" to #";
		fo<<jmax;
	}

	fo<<" at z=";
	s=s.FormatFloat("#0.00",100*Structure[Si].ksi*Structure[Si].lmb);
	fo<<s.c_str();
	fo<<" cm\n";

	fo<<"Particle #\t";
	if (!BeamExport[Sn].LiveOnly)
		fo<<"Lost\t";
	if (BeamExport[Sn].Phase)
		fo<<"Phase [deg]\t";
	if (BeamExport[Sn].Energy)
		fo<<"Energy [MeV]\t";
	if (BeamExport[Sn].Radius)
		fo<<"Radius [mm]\t";
	if (BeamExport[Sn].Azimuth)
		fo<<"Azimuth [deg]\t";
	if (BeamExport[Sn].Divergence)
		fo<<"Divergence	[mrad]\t";
				//fo<<"Vth [m/s]\t";
	fo<<"\n";
}
//---------------------------------------------------------------------------
void TBeamSolver::DumpFile(ofstream &fo,int Sn,int j)
{
	AnsiString s;
	int Si=BeamExport[Sn].Nmesh;

	if(!BeamExport[Sn].LiveOnly || (BeamExport[Sn].LiveOnly && !Beam[Si]->Particle[j].lost)){
		s=s.FormatFloat("#0000\t\t",j+1);
		fo<<s.c_str();

		if (!BeamExport[Sn].LiveOnly) {
			if (Beam[Si]->Particle[j].lost)
				fo<<"LOST\t";
			else
				fo<<"LIVE\t";
		}
		if (BeamExport[Sn].Phase){
			s=s.FormatFloat("#0.00\t\t",RadToDegree(Beam[Si]->GetParameter(j,PHI_PAR)));
			fo<<s.c_str();
		}
		if (BeamExport[Sn].Energy){
			s=s.FormatFloat("#0.00\t\t\t",Beam[Si]->GetParameter(j,W_PAR));
			fo<<s.c_str();
		}
		if (BeamExport[Sn].Radius){
			s=s.FormatFloat("#0.00\t\t\t",1000*Beam[Si]->GetParameter(j,R_PAR));
			fo<<s.c_str();
		}
		if (BeamExport[Sn].Azimuth){
			s=s.FormatFloat("#0.00\t\t",RadToDegree(Beam[Si]->GetParameter(j,TH_PAR)));
			fo<<s.c_str();
		}
		if (BeamExport[Sn].Divergence){
			s=s.FormatFloat("#0.00\t\t",1000*Beam[Si]->GetParameter(j,AR_PAR));
			fo<<s.c_str();
		}
		fo<<"\n";
	}
}
//---------------------------------------------------------------------------
void TBeamSolver::DumpASTRA(ofstream &fo,int Sn,int j,int jref)
{
	AnsiString s;
	double x=0, y=0, z=0;
	double px=0, py=0, pz=0;
	double q=0, I=0, clock=0;
	int  status=0, index=1;   //1 - electrons, 2 - positrons, 3 - protons, 4 - heavy ions;

	int Si=BeamExport[Sn].Nmesh;

	x=Beam[Si]->GetParameter(j,X_PAR);
	y=Beam[Si]->GetParameter(j,Y_PAR);
	z=Beam[Si]->GetParameter(j,PHI_PAR)*Structure[Si].lmb/(2*pi);
	clock=Beam[Si]->GetParameter(j,PHI_PAR)*Structure[Si].lmb/(2*pi*c);

	double W=Beam[Si]->GetParameter(j,W_PAR);
	double gamma=MeVToGamma(W);

	px=We0*gamma*Beam[Si]->GetParameter(j,BX_PAR);
	py=We0*gamma*Beam[Si]->GetParameter(j,BY_PAR);
	pz=We0*gamma*Beam[Si]->GetParameter(j,BZ_PAR);

	I=BeamPar.Current/BeamPar.NParticles;
	q=I*Structure[Si].lmb/c;

	if (j==jref) {
	   x=0;
	   y=0;
	   z=0;

	   clock=0;
	   status=5;  //reference particle
	}  else {
	   z=z-Beam[Si]->GetParameter(jref,PHI_PAR)*Structure[Si].lmb/(2*pi);
	   pz=pz-We0*gamma*Beam[Si]->GetParameter(jref,BZ_PAR);
	   clock=clock-Beam[Si]->GetParameter(jref,PHI_PAR)*Structure[Si].lmb/(2*pi*c);

	   status=3; //regular
	}

	s=s.FormatFloat("#.##############e+0 ",x);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",y);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",z);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",px);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",py);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",pz);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",1e9*clock);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",1e9*q);
	fo<<s.c_str();
	s=s.FormatFloat("0 ",index);
	fo<<s.c_str();
	s=s.FormatFloat("0 ",status);
	fo<<s.c_str();

	fo<<"\n";
}
//---------------------------------------------------------------------------
void TBeamSolver::DumpCST(ofstream &fo,int Sn,int j)
{
	AnsiString s;
	double x=0, y=0, z=0;
	double px=0, py=0, pz=0;
	double m=0, q=0, I=0,t=0;

	int Si=BeamExport[Sn].Nmesh;

	x=Beam[Si]->GetParameter(j,X_PAR);
	y=Beam[Si]->GetParameter(j,Y_PAR);
	t=Beam[Si]->GetParameter(j,PHI_PAR)*Structure[Si].lmb/(2*pi*c);

	double W=Beam[Si]->GetParameter(j,W_PAR);
	double p=MeVToBetaGamma(W);
	double beta=Beam[Si]->GetParameter(j,BETA_PAR);
	px=p*Beam[Si]->GetParameter(j,BX_PAR)/beta;
	py=p*Beam[Si]->GetParameter(j,BY_PAR)/beta;
	pz=p*Beam[Si]->GetParameter(j,BZ_PAR)/beta;

	m=me;
	q=qe;
	I=BeamPar.Current/BeamPar.NParticles;
	s=s.FormatFloat("#.##############e+0 ",x);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",y);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",z);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",px);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",py);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",pz);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",m);
	fo<<s.c_str();
	s=s.FormatFloat("#.##############e+0 ",q);
	fo<<s.c_str();
	if (BeamExport[Sn].SpecialFormat==CST_PID) {
		s=s.FormatFloat("#.##############e+0",I);
		fo<<s.c_str();
	} else if (BeamExport[Sn].SpecialFormat==CST_PIT) {
		s=s.FormatFloat("#.##############e+0 ",I*Structure[Si].lmb/c);
		fo<<s.c_str();
		s=s.FormatFloat("#.##############e+0",t);
		fo<<s.c_str();
	}
	fo<<"\n";

}
//---------------------------------------------------------------------------
void TBeamSolver::DumpBeam(int Sn)
{
	int Si=BeamExport[Sn].Nmesh;
	AnsiString F=BeamExport[Sn].File.c_str();
	AnsiString s;

	switch (BeamExport[Sn].SpecialFormat) {
		case CST_PID:{F+=".pid";break;}
		case CST_PIT:{F+=".pit";break;}
		case ASTRA:{F+=".ini";break;}
		case NOBEAM:{}
		default: {F+=".dat";break;}
	}

	std::ofstream fo(F.c_str());

	int jmin=0;
	int jmax=BeamPar.NParticles;

	if (BeamExport[Sn].N1>0 && BeamExport[Sn].N2==0) {
		jmin=0;
		jmax=BeamExport[Sn].N1;
	} else if (BeamExport[Sn].N1>0 && BeamExport[Sn].N2>0) {
		jmin=BeamExport[Sn].N1-1;
		jmax=BeamExport[Sn].N2;
	}

	if (jmin>BeamPar.NParticles || jmax>BeamPar.NParticles) {
		if (BeamExport[Sn].SpecialFormat==NOBEAM)
			fo<<"WARNING: The defined range of particle numbers exceeds the number of available particles. The region was set to default.\n";
		jmin=0;
		jmax=BeamPar.NParticles;
	}
	switch (BeamExport[Sn].SpecialFormat) {
		case CST_PIT: {}
		case CST_PID: {
			for (int j=jmin;j<jmax;j++)
				DumpCST(fo,Sn,j);
			break;
		}
		case ASTRA: {
			for (int j=jmin;j<jmax;j++)
				DumpASTRA(fo,Sn,j,jmin);
			break;
		}
		case NOBEAM:{}
		default:{
			DumpHeader(fo,Sn,jmin,jmax);
			for (int j=jmin;j<jmax;j++)
				DumpFile(fo,Sn,j);
		}
	}
	fo.close();
}
//---------------------------------------------------------------------------
void TBeamSolver::Step(int Si)
{
    bool drift=false;
	double lmb=Structure[Si].lmb;
    Beam[Si]->lmb=lmb;
	CountLiving(Si);
	I=BeamPar.Current*Nliv/BeamPar.NParticles;
  /*
    Rb=Beam[i]->GetBeamRadius();
    phi0=Beam[i]->GetAveragePhase();
    dphi=Beam[i]->GetPhaseLength();
    Lb=dphi*lmb/(2*pi);
    betta0=Beam[i]->GetAverageEnergy();

    w=Structure[i]->alpha*lmb;        */
    drift=(Structure[Si].drift);
    for (int i=0;i<4;i++)
        Par[i].drift=Structure[Si].drift;
    //Par[3].drift=Structure[Si+1].drift;

    dh=Structure[Si+1].ksi-Structure[Si].ksi;
    Par[0].h=0;
    Par[1].h=dh/2;
    Par[2].h=Par[1].h;
	Par[3].h=dh;

	double db=Structure[Si+1].betta-Structure[Si].betta;
    Par[0].bw=Structure[Si].betta;
    Par[1].bw=Structure[Si].betta+db/2;
    Par[2].bw=Par[1].bw;
    Par[3].bw=Structure[Si+1].betta;

	double dw=Structure[Si+1].alpha-Structure[Si].alpha;
    Par[0].w=Structure[Si].alpha*lmb;
    Par[1].w=(Structure[Si].alpha+dw/2)*lmb;
    Par[2].w=Par[1].w;
    Par[3].w=Structure[Si+1].alpha*lmb;

    double dE=Structure[Si+1].E-Structure[Si].E;
    Par[0].E=Structure[Si].E;
    Par[1].E=Structure[Si].E+dE/2;
    Par[2].E=Par[1].E;
    Par[3].E=Structure[Si+1].E;

    double dA=Structure[Si+1].A-Structure[Si].A;
    Par[0].A=Structure[Si].A;
    Par[1].A=Structure[Si].A;//+dA/2;
    Par[2].A=Par[1].A;
    Par[3].A=Structure[Si].A;

    double dB=Structure[Si+1].B-Structure[Si].B;
    Par[0].B=Structure[Si].B;
    Par[1].B=Structure[Si].B+dB/2;
    Par[2].B=Par[1].B;
    Par[3].B=Structure[Si+1].B;

   /*   for(int i=0;i<4;i++)
        Par[i].B*=I;  */

    double d2E=0;
    double d2A=0;
	double d2h=0;
	double d2B=0;
	double dR=0;
    if (Structure[Si+1].Rp!=0 && Structure[Si].Rp!=0)
        dR=ln(Structure[Si+1].Rp)-ln(Structure[Si].Rp);
    double d2R=0;

    if (drift){
        for (int i=0;i<4;i++){
            Par[i].dL=0;
            Par[i].dA=0;
        }
    } else {
		if (Si==0 || (Si!=0 && Structure[Si].jump)){
			Par[0].dL=dE/(Structure[Si].E*dh);
			//Par[0].dL=dR/dh;
			Par[0].dA=dA/dh;
		}else{
			d2E=Structure[Si+1].E-Structure[Si-1].E;
			d2A=Structure[Si+1].A-Structure[Si-1].A;
			d2h=Structure[Si+1].ksi-Structure[Si-1].ksi;
			Par[0].dL=d2E/(Structure[Si].E*d2h);
		   //   d2R=ln(Structure[Si+1].Rp)-ln(Structure[Si-1].Rp);
		   //   Par[0].dL=d2R/d2h;
			Par[0].dA=d2A/d2h;
		}

        Par[1].dL=dE/((Structure[Si].E+dE/2)*dh);
        //Par[1].dL=dR/dh;
        Par[2].dL=Par[1].dL;

		Par[1].dA=dA/dh;
		Par[2].dA=Par[1].dA;

		if (Si==Npoints-2 || (Si<Npoints-2 && Structure[Si+2].jump)){
            Par[3].dL=dE/(Structure[Si+1].E*dh);
            //Par[3].dL=dR/dh;
			Par[3].dA=dA/dh;
		}else{
           //   d2E=Structure[Si+2].E-Structure[Si].E;
			d2A=Structure[Si+2].A-Structure[Si].A;
			d2h=Structure[Si+2].ksi-Structure[Si].ksi;
            Par[3].dL=d2E/(Structure[Si+1].E*d2h);
            //d2R=ln(Structure[Si+2].Rp)-ln(Structure[Si].Rp);
            ///Par[0].dL=d2R/d2h;
			Par[3].dA=d2A/d2h;
        }
    }

	double dBzx=Structure[Si+1].Hext.z-Structure[Si].Hext.z;
	Par[0].Hext.z=Structure[Si].Hext.z;
	Par[1].Hext.z=Structure[Si].Hext.z+dBzx/2;
	Par[2].Hext.z=Par[1].Hext.z;
	Par[3].Hext.z=Structure[Si+1].Hext.z;

	double dBrx=Structure[Si+1].Hext.r-Structure[Si].Hext.r;
	Par[0].Hext.r=Structure[Si].Hext.r;
	Par[1].Hext.r=Structure[Si].Hext.r+dBrx/2;
	Par[2].Hext.r=Par[1].Hext.r;
	Par[3].Hext.r=Structure[Si+1].Hext.r;

	double dBthx=Structure[Si+1].Hext.th-Structure[Si].Hext.th;
	Par[0].Hext.th=Structure[Si].Hext.th;
	Par[1].Hext.th=Structure[Si].Hext.th+dBthx/2;
	Par[2].Hext.th=Par[1].Hext.th;
	Par[3].Hext.th=Structure[Si+1].Hext.th;

  /*	if (Si==0)
		Par[0].dH=dBzx/dh;
	else{
		d2Bzx=Structure[Si+1].Hext.z-Structure[Si-1].Hext.z;
		d2h=Structure[Si+1].ksi-Structure[Si-1].ksi;
		Par[0].dH=d2Bzx/d2h;
	}
	Par[1].dH=dBzx/dh;
	Par[2].dH=Par[1].dH;
	if (Si==Npoints-2){
		Par[3].dH=dBzx/dh;
	} else {
		d2Bzx=Structure[Si+2].Bz_ext-Structure[Si].Bz_ext;
		d2h=Structure[Si+2].ksi-Structure[Si].ksi;
		Par[3].dH=d2Bzx/d2h;
	}

   	FILE *logFile=fopen("beam.log","a");
	//fprintf(logFile,"Phase Radius Betta\n");
    //for (int i=0;i<Np;i++)
		fprintf(logFile,"%i %f %f %f %f\n",Si,Par[0].dH,Par[1].dH,Par[2].dH,Par[3].dH);

	fclose(logFile);   */

    for (int j=0;j<Ncoef;j++)
        Integrate(Si,j);
}
//---------------------------------------------------------------------------
void TBeamSolver::Solve()
{
	#ifndef RSLINAC
	if (SmartProgress==NULL){
		ShowMessage("System Message: ProgressBar not assigned! Code needs to be corrected");
        return;
    }
    SmartProgress->Reset(Npoints-1/*Np*/);
    #endif

  //    logFile=fopen("beam.log","w");
 /* for (int i=0;i<Np;i++){
      //    fprintf(logFile,"%f %f\n",Beam[0]->Particle[i].x,Beam[0]->Particle[i].x/Structure[0].lmb);
        Beam[0]->Particle[i].x/=Structure[0].lmb;
    }                                             */
 // fclose(logFile);

    for (int i=0;i<Ncoef;i++){
        delete[] K[i];
		K[i]=new TIntegration[BeamPar.NParticles];
    }
    K[0][0].A=Structure[0].A;
   //   Beam[0]->Particle[j].z=Structure[0].ksi*Structure[0].lmb;
   //
	for (int i=0;i<Npoints;i++){
        //for (int j=0;j<Np;j++){

            //if (i==0)
           //   Nliv=Beam[i]->GetLivingNumber();
		for (int j = 0; j < Ndump; j++) {
			if (BeamExport[j].Nmesh==i) {
				DumpBeam(j);
			}
		}

		if (i==Npoints-1) break; // Nowhere to iterate

		if (!Structure[i+1].jump){
			Step(i);
			Structure[i+1].A=Structure[i].A+dh*(K[0][0].A+K[1][0].A+2*K[2][0].A+2*K[3][0].A)/6;
			//  fprintf(logFile,"%f %f %f %f %f\n",K[1][0].A,K[2][0].A,K[3][0].A,K[0][0].A,Structure[i+1].A);
			Beam[i]->Next(Beam[i+1],Par[3],K);
		} else {
			//Structure[i+1].A=Structure[i].A ;
			Beam[i]->Next(Beam[i+1]);
		}

		for (int j=0;j<BeamPar.NParticles;j++){
			if (Beam[i+1]->Particle[j].lost==LIVE && mod(Beam[i+1]->Particle[j].r)>=Structure[i+1].Ra)
				Beam[i+1]->Particle[j].lost=RADIUS_LOST;
			Beam[i+1]->Particle[j].z=Structure[i+1].ksi*Structure[i+1].lmb;
			Beam[i+1]->Particle[j].phi-=Structure[i+1].dF;
		}

		#ifndef RSLINAC
		SmartProgress->operator ++();
		Application->ProcessMessages();
		#endif
		if (Stop){
			Stop=false;
			#ifndef RSLINAC
			AnsiString S="Solve Process Aborted!";
			ShowError(S);
			SmartProgress->Reset();
			#endif
			return;
        }
        for (int i=0;i<Ncoef;i++)
            memset(K[i], 0, sizeof(TIntegration));
        //}
    }

   //

    #ifndef RSLINAC
    SmartProgress->SetPercent(100);
    SmartProgress->SetTime(0);
    #endif
}
//---------------------------------------------------------------------------
#ifndef RSLINAC
TResult TBeamSolver::Output(AnsiString& FileName,TMemo *Memo)
#else
TResult TBeamSolver::Output(AnsiString& FileName)
#endif
{
    AnsiString Line,s;
    TStringList *OutputStrings;
    OutputStrings= new TStringList;
    TResult Result;
  /*    int Nliv=0;
    Nliv=Beam[Npoints-1]->GetLivingNumber();
                     */
    OutputStrings->Clear();
	OutputStrings->Add("========================================");
    OutputStrings->Add("INPUT DATA from:");
	OutputStrings->Add(InputFile);
	OutputStrings->Add("========================================");

  /*    TStringList *InputStrings;
    InputStrings= new TStringList;
	InputStrings->LoadFromFile(InputFile);    */
    OutputStrings->AddStrings(InputStrings);

	OutputStrings->Add("========================================");
    OutputStrings->Add("RESULTS");
	OutputStrings->Add("========================================");
    OutputStrings->Add("");

    double Ws=0;
   //   AnsiString s;
    int j=Npoints-1;
    double z=100*Structure[j].ksi*Structure[j].lmb;

	TGauss Gw=GetEnergyStats(j,D_FWHM);

	double Wm=Beam[j]->GetMaxEnergy();
	double I=Beam[j]->GetCurrent();
	double I0=Beam[0]->GetCurrent();
    double kc=100.0*Beam[j]->GetLivingNumber()/Beam[0]->GetLivingNumber();
	double r=1e3*Beam[j]->GetRadius();

	TGauss Gphi=GetPhaseStats(j,D_FWHM);

    double f=1e-6*c/Structure[j].lmb;
	double Ra=1e3*Structure[j].Ra*Structure[j].lmb;
	double P=Gw.mean*I;

	double W0=Beam[0]->GetAverageEnergy();
	//double Wout=Beam[j]->GetAverageEnergy();
	double Pin=W0*I0;


	double v=Structure[j].betta;
	double E=sqrt(2*Structure[j].Rp);
	double Pb=E!=0?1e-6*sqr(Structure[j].A*We0/E):0;

	double beta_gamma=MeVToVelocity(Gw.mean)*MeVToGamma(Gw.mean);

    /*double Pw=P0;
    for(int i=1;i<Npoints;i++)
        Pw=Pw*exp(-2*(Structure[i].ksi-Structure[i-1].ksi)*Structure[i].alpha*Structure[i].lmb);  */
	//double Pw=1e-6*P0-(P-Pin+Pb);

	TTwiss TR,TX,TY;
	TR=GetTwiss(j,R_PAR);
	TX=GetTwiss(j,X_PAR);
	TY=GetTwiss(j,Y_PAR);
  /*  double A=0;
  	int Na=j-Nmesh/2;
	if (Na>0)
		A=Structure[Na].A;     */

    Result.Length=z;
	Result.MaximumEnergy=Wm;
	Result.Energy=Gw;
	Result.InputCurrent=1e3*I0;
    Result.BeamCurrent=1e3*I;
    Result.Captured=kc;
	Result.BeamRadius=r;
	Result.Phase=Gphi;
    Result.BeamPower=P;
	Result.LoadPower=Pb;
	Result.RTwiss=TR;
	Result.XTwiss=TX;
	Result.YTwiss=TY;
  //  Result.A=A;

	Line="Total Length = "+s.FormatFloat("#0.000",Result.Length)+" cm";
    OutputStrings->Add(Line);
	Line="Average Energy = "+s.FormatFloat("#0.000",Result.Energy.mean)+" MeV";
    OutputStrings->Add(Line);
    Line="Maximum Energy = "+s.FormatFloat("#0.000",Result.MaximumEnergy)+" MeV";
    OutputStrings->Add(Line);
	Line="Energy Spectrum (FWHM) = "+s.FormatFloat("#0.00",100*Result.Energy.FWHM/Result.Energy.mean)+" %";
	OutputStrings->Add(Line);
    Line="Input Current = "+s.FormatFloat("#0.00",Result.InputCurrent)+" mA";
    OutputStrings->Add(Line);
    Line="Beam Current = "+s.FormatFloat("#0.00",Result.BeamCurrent)+" mA";
    OutputStrings->Add(Line);
	Line="Particles Transmitted = "+s.FormatFloat("#0.00",Result.Captured)+" %";
	OutputStrings->Add(Line);
    Line="Beam Radius (RMS) = "+s.FormatFloat("#0.00",Result.BeamRadius)+" mm";
    OutputStrings->Add(Line);
	Line="Average Phase = "+s.FormatFloat("#0.00",RadToDegree(Result.Phase.mean))+" deg";
	OutputStrings->Add(Line);
	Line="Phase Length (FWHM) = "+s.FormatFloat("#0.00",RadToDegree(Result.Phase.FWHM))+" deg";
	OutputStrings->Add(Line);
	OutputStrings->Add("Twiss Parameters (R):");
	Line="alpha= "+s.FormatFloat("#0.00000",Result.RTwiss.alpha);
	OutputStrings->Add(Line);
	Line="betta = "+s.FormatFloat("#0.00000",100*Result.RTwiss.beta)+" cm/rad";
	OutputStrings->Add(Line);
	Line="emittance = "+s.FormatFloat("#0.000000",1e6*Result.RTwiss.epsilon)+" um";
	OutputStrings->Add(Line);
	Line="emittance (norm) = "+s.FormatFloat("#0.000000",1e6*beta_gamma*Result.RTwiss.epsilon)+" um";
	OutputStrings->Add(Line);
	OutputStrings->Add("Twiss Parameters (X):");
	Line="alpha= "+s.FormatFloat("#0.00000",Result.XTwiss.alpha);
	OutputStrings->Add(Line);
	Line="betta = "+s.FormatFloat("#0.00000",100*Result.XTwiss.beta)+" cm/rad";
	OutputStrings->Add(Line);
	Line="emittance = "+s.FormatFloat("#0.000000",1e6*Result.XTwiss.epsilon)+" um";
	OutputStrings->Add(Line);
	Line="emittance (norm) = "+s.FormatFloat("#0.000000",1e6*beta_gamma*Result.XTwiss.epsilon)+" um";
	OutputStrings->Add(Line);
	OutputStrings->Add("Twiss Parameters (Y):");
	Line="alpha= "+s.FormatFloat("#0.00000",Result.YTwiss.alpha);
	OutputStrings->Add(Line);
	Line="betta = "+s.FormatFloat("#0.00000",100*Result.YTwiss.beta)+" cm/rad";
	OutputStrings->Add(Line);
	Line="emittance = "+s.FormatFloat("#0.000000",1e6*Result.YTwiss.epsilon)+" um";
	OutputStrings->Add(Line);
	Line="emittance (norm) = "+s.FormatFloat("#0.000000",1e6*beta_gamma*Result.YTwiss.epsilon)+" um";
	OutputStrings->Add(Line);
	//OutputStrings->Add("========================================");

	int Nsec=0;
	double Pbeam0=0, Wb0=0;
	double Ib=0, Wb=0, P0=0;
	for (int i=0;i<=j;i++){
		if ((Structure[i].jump && !Structure[i].drift) || i==j) {
			Ib=Beam[i]->GetCurrent();
			Wb=Beam[i]->GetAverageEnergy();

			if (Nsec>0) {
				double Pload=E!=0?1e-6*sqr(Structure[i-1].A*We0/E):0;
				double Pbeam=Ib*Wb;
				double F0=1e-6*GetSectionFrequency(Nsec-1);

				Line="===========Section #"+s.FormatFloat("#",Nsec)+" ======================";
				OutputStrings->Add(Line);
				Line="Input Power = "+s.FormatFloat("#0.0000",P0)+" MW";
				OutputStrings->Add(Line);
				Line="Frequency = "+s.FormatFloat("#0.0000",F0)+" MHz";
				OutputStrings->Add(Line);
			   /*	Line="Beam Power = "+s.FormatFloat("#0.0000",Pbeam)+" MW";
				OutputStrings->Add(Line);
				Line="Beam Energy = "+s.FormatFloat("#0.0000",Wb)+" MeV";
				OutputStrings->Add(Line);   */
				Line="Beam Energy Gain = "+s.FormatFloat("#0.0000",Wb-Wb0)+" MeV";
				OutputStrings->Add(Line);
				Line="Beam Power Gain = "+s.FormatFloat("#0.0000",Pbeam-Pbeam0)+" MW";
				OutputStrings->Add(Line);
				Line="Load Power = "+s.FormatFloat("#0.0000",Pload)+" MW";
				OutputStrings->Add(Line);
				Line="Loss Power = "+s.FormatFloat("#0.0000",P0-(Pbeam-Pbeam0+Pload))+" MW";
				OutputStrings->Add(Line);
			}

			Pbeam0=Ib*Wb;
			P0=1e-6*Structure[i].P;
			Wb0=Wb;
			Nsec++;
		}
	}
	OutputStrings->Add("========================================");


    #ifndef RSLINAC
	if (Memo!=NULL){
        Memo->Lines->AddStrings(OutputStrings);
    }
    #endif

    OutputStrings->SaveToFile(FileName);
    delete OutputStrings;
   //   delete Strings;



    return Result;
}
//---------------------------------------------------------------------------
#pragma package(smart_init)
