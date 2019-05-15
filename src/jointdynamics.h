﻿#ifndef JOINTDYNAMICS_H_
#define JOINTDYNAMICS_H_

#include <array>


	


namespace JointDynamicsInt
{ 
	const int LoadTotalParas = 40;
	const int LoadReduceParas = 15;
	const int JointGroupDim = 60;
	const int JointReduceDim = 30;
   

	class jointdynamics
	{
	public:

		double A[3][3];
		double B[3];

    double estParasL[LoadReduceParas+6] = { 0 };
	double CoefParasL[LoadReduceParas* LoadTotalParas] = { 0 };
	double estParasL0[LoadReduceParas+6] = { 0 };
	double CoefParasL0[LoadReduceParas* LoadTotalParas] = { 0 };
    double LoadParas[10] = { 0 };

	double estParasJoint0[JointReduceDim+12] = { 0 };
	double estParasJoint[JointReduceDim] = { 0 };
	double CoefParasJoint[JointReduceDim* JointGroupDim] = { 0 };
	double CoefParasJointInv[JointReduceDim* JointGroupDim] = { 0 };

        jointdynamics();
        void JointCollision(const double * q, const double *dq,const double *ddq,const double *ts, const double *estParas, const double * CoefInv, const double * Coef, const double * LoadParas, double * CollisionFT);
        void RLS(const double *positionList, const double *sensorList, double *estParas, double *Coef, double *CoefInv, double *StatisError);
		void LoadRLS(const double *positionList, const double *sensorList, double *estParas, double *Coef,double *StatisError);
		void LoadParasExt(const double *dEst, const double *dCoef, double *Load);

		//void sixDistalMatrix(const double * q, const double *dq,const double *ddq,const double *ts,double );
		
	};

	
}

#endif