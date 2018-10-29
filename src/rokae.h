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
	//const std::string xmlpath = "C:\\Users\\kevin\\Desktop\\aris_rokae\\ServoPressorCmdList.xml";
	constexpr double ea_a = 3765.8, ea_b = 1334.8, ea_c = 45.624, ea_gra = 4, ea_index = 6;  //��׵�������ѹ����ϵ����ea_k��ʾ����ϵ����ea_b��ʾ�ؾ࣬ea_offset��ʾ����Ӱ������ea_index��ʾ����Ť��ϵ��=�Ť��*6.28*���ٱ�/����/1000//
	using Size = std::size_t;
	constexpr double PI = 3.141592653589793;
	auto createModelRokaeXB4(const double *robot_pm = nullptr)->std::unique_ptr<aris::dynamic::Model>;
	auto createControllerRokaeXB4()->std::unique_ptr<aris::control::Controller>;
	auto createPlanRootRokaeXB4()->std::unique_ptr<aris::plan::PlanRoot>;
}


#endif