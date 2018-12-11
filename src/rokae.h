#ifndef ROKAE_H_
#define ROKAE_H_

#include <memory>

#include <aris_control.h>
#include <aris_dynamic.h>
#include <aris_plan.h>
#include <tinyxml2.h>  


//statemachine//
# define M_RUN 0	//�ֶ�����ִ��
# define READ_RT_DATA 1		//���ʵʱ����
# define READ_XML 2		//���ʵʱ����
# define A_RUN 3	//�Զ�ִ��
# define A_QUIT 4	//�˳��Զ�ִ�У����ص��ֶ�ģʽ
# define buffer_length 800
//statemachine//

/// \brief �����������ռ�
/// \ingroup aris
/// 

namespace rokae
{
	//���������������
	//const std::string xmlpath = "C:\\Users\\kevin\\Desktop\\aris_rokae\\ServoPressorCmdList.xml";
	constexpr double ea_a = 3765.8, ea_b = 1334.8, ea_c = 45.624, ea_gra = 24, ea_index = -6, ea_gra_index = 36;  //��׵�������ѹ����ϵ����ea_k��ʾ����ϵ����ea_b��ʾ�ؾ࣬ea_offset��ʾ����Ӱ������ea_index��ʾ����Ť��ϵ��=�Ť��*6.28*���ٱ�/����/1000//
	
	//���������ز�������
	constexpr double f_static[6] = { 0.116994475,0.139070885,0.057812486,0.04834123,0.032697209,0.03668566 };
	constexpr double f_vel[6] = { 0.091826484,0.189104972,0.090449316,0.044415268,0.015864525,0.007350605 };
	constexpr double f_acc[6] = { 0.011658463,0.044943276,0.005936147,0.002210092,0.000618672,0.000664163 };
	constexpr double f2c_index = 5378.33927639823, max_static_vel = 0.1, f_static_index = 0.8;
	
	//���������ͺ�������
	using Size = std::size_t;
	constexpr double PI = 3.141592653589793;
	auto createModelRokaeXB4(const double *robot_pm = nullptr)->std::unique_ptr<aris::dynamic::Model>;
	auto createControllerRokaeXB4()->std::unique_ptr<aris::control::Controller>;
	auto createPlanRootRokaeXB4()->std::unique_ptr<aris::plan::PlanRoot>;
}


#endif