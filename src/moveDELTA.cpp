#include<iostream>
//��������sris���ͷ�ļ�
#include<aris.h>

using namespace std;
//����aris���е�planģ��
using namespace aris::plan;

//����ethercat��վ������controller��������xml�ļ���Ӵ�վ��Ϣ
/*�������ص���һ����ָ�룬ָ��ָ��Controller,controller������������ָ��std::unique_ptr*/
auto createControllerRokaeXB4()->std::unique_ptr<aris::control::Controller>	
{
	/*����std::unique_ptrʵ��*/
	std::unique_ptr<aris::control::Controller> controller(new aris::control::EthercatController);
	/*����Ethercat��������xmal�ļ�*/

	std::string xml_str =
        "<m_servo_press type=\"EthercatMotion\" phy_id=\"0\" product_code=\"0x10305070\""
		" vendor_id=\"0x000001DD\" revision_num=\"0x02040608\" dc_assign_activate=\"0x0300\""
        " min_pos=\"-205783.04\" max_pos=\"205776.76\" max_vel=\"314\" min_vel=\"-314\""
		" max_acc=\"3140\" min_acc=\"-3140\" max_pos_following_error=\"0.005\" max_vel_following_error=\"0.005\""
        " home_pos=\"0\" pos_factor=\"166970.70\" pos_offset=\"0.0\">"
		"	<sm_pool type=\"SyncManagerPoolObject\">"
		"		<sm type=\"SyncManager\" is_tx=\"false\"/>"
		"		<sm type=\"SyncManager\" is_tx=\"true\"/>"
		"		<sm type=\"SyncManager\" is_tx=\"false\">"
		"			<index_1600 type=\"Pdo\" default_child_type=\"PdoEntry\" index=\"0x1600\" is_tx=\"false\">"
		"				<control_word index=\"0x6040\" subindex=\"0x00\" size=\"16\"/>"
		"				<mode_of_operation index=\"0x6060\" subindex=\"0x00\" size=\"8\"/>"
		"				<target_pos index=\"0x607A\" subindex=\"0x00\" size=\"32\"/>"
		"				<target_vel index=\"0x60FF\" subindex=\"0x00\" size=\"32\"/>"
		"				<offset_vel index=\"0x60B1\" subindex=\"0x00\" size=\"32\"/>"
		"				<targer_tor index=\"0x6071\" subindex=\"0x00\" size=\"16\"/>"
		"				<offset_tor index=\"0x60B2\" subindex=\"0x00\" size=\"16\"/>"
		"			</index_1600>"
		"		</sm>"
		"		<sm type=\"SyncManager\" is_tx=\"true\">"
		"			<index_1a00 type=\"Pdo\" default_child_type=\"PdoEntry\" index=\"0x1A00\" is_tx=\"true\">"
		"				<status_word index=\"0x6041\" subindex=\"0x00\" size=\"16\"/>"
		"				<mode_of_display index=\"0x6061\" subindex=\"0x00\" size=\"8\"/>"
		"				<pos_actual_value index=\"0x6064\" subindex=\"0x00\" size=\"32\"/>"
		"				<vel_actual_value index=\"0x606c\" subindex=\"0x00\" size=\"32\"/>"
		"				<cur_actual_value index=\"0x6078\" subindex=\"0x00\" size=\"16\"/>"
		"			</index_1a00>"
		"		</sm>"
		"	</sm_pool>"
		"	<sdo_pool type=\"SdoPoolObject\" default_child_type=\"Sdo\">"
		"	</sdo_pool>"
		"</m_servo_press>";
	controller->slavePool().add<aris::control::EthercatMotion>().loadXmlStr(xml_str);

	return controller;
};

// �����켣
struct MoveDELTAParam
{
	std::vector<bool> axis_active_vec;
	std::vector<Size> total_count_vec;
	std::vector<double> axis_begin_pos_vec;
	std::vector<double> axis_pos_vec;
	std::vector<double> axis_vel_vec;
	std::vector<double> axis_acc_vec;
	std::vector<double> axis_dec_vec;
	bool abs;
};
class MoveDELTA : public aris::plan::Plan
{
public:
	auto virtual prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
	{
		auto c = dynamic_cast<aris::control::Controller*>(target.master);
		MoveDELTAParam param;
		param.begin_joint_pos_vec.resize(c->motionPool().size());

		aris::core::Calculator cal;

		for (auto &p : params)
		{
			if (p.first == "all")
			{
				param.axis_active_vec.resize(c->motionPool().size(), true);
			}
			else if (p.first == "motion_id")
			{
				param.axis_active_vec.resize(c->motionPool().size(), false);
				param.axis_active_vec.at(std::stoi(p.second)) = true;
			}
			else if (p.first == "pos")
			{
				aris::core::Matrix mat = cal->calculator().calculateExpression(p.second);
				if (mat.size() == 1)param.axis_pos_vec.resize(c->motionPool().size(), mat.toDouble());
				else
				{
					param.axis_pos_vec.resize(mat.size());
					std::copy(mat.begin(), mat.end(), param.axis_pos_vec.begin());
				}
			}
			else if (p.first == "vel")
			{	
				auto v = cal->calculator().calculateExpression(p.second);
				if (v.size() == 1)
				{
					param.axis_vel_vec.resize(c->motionPool().size(), v.toDouble());
				}
				else if (v.size() == c->motionPool().size())
				{
					param.axis_vel_vec.assign(v.begin(), v.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}
				//���ٶ�ֵ���޽��б���
				for (Size i = 0; i < param.axis_vel_vec.size(); ++i)
				{
					if (param.axis_vel_vec[i] > 0.2)
					{
						param.axis_vel_vec[i] = 0.2;
					}
					if (param.axis_vel_vec[i] < -0.2)
					{
						param.axis_vel_vec[i] = -0.2;
					}
				}
			}
			else if (p.first == "acc")
			{
				auto a = cal->calculator().calculateExpression(p.second);
				if (a.size() == 1)
				{
					param.axis_acc_vec.resize(c->motionPool().size(), a.toDouble());
				}
				else if (a.size() == c->motionPool().size())
				{
					param.axis_acc_vec.assign(a.begin(), a.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}
				//�Լ��ٶ�ֵ���޽��б���
				for (Size i = 0; i < param.axis_acc_vec.size(); ++i)
				{
					if (param.axis_acc_vec[i] > 2.0)
					{
						param.axis_acc_vec[i] = 2.0;
					}
					if (param.axis_acc_vec[i] < -2.0)
					{
						param.axis_acc_vec[i] = -2.0;
					}
				}
			}
			else if (p.first == "dec")
			{
				auto d = cal->calculator().calculateExpression(p.second);
				if (d.size() == 1)
				{
					param.axis_dec_vec.resize(c->motionPool().size(), d.toDouble());
				}
				else if (d.size() == c->motionPool().size())
				{
					param.axis_dec_vec.assign(d.begin(), d.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}
				//�Լ��ٶ�ֵ���޽��б���
				for (Size i = 0; i < param.axis_dec_vec.size(); ++i)
				{
					if (param.axis_dec_vec[i] > 2.0)
					{
						param.axis_dec_vec[i] = 2.0;
					}
					if (param.axis_dec_vec[i] < -2.0)
					{
						param.axis_dec_vec[i] = -2.0;
					}
				}
			}
			else if (p.first == "abs")
			{
				param.abs = std::stod(p.second);
			}
		}

		target.param = param;

		target.option =
			Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER |
			Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER_AT_START |
			Plan::NOT_CHECK_POS_FOLLOWING_ERROR |
			Plan::NOT_CHECK_VEL_CONTINUOUS |
			Plan::NOT_CHECK_VEL_CONTINUOUS_AT_START |
			Plan::NOT_CHECK_VEL_FOLLOWING_ERROR;

	}
	auto virtual executeRT(PlanTarget &target)->int
	{
		auto &param = std::any_cast<MoveDELTAParam&>(target.param);
		// ������վ //
		auto controller = dynamic_cast<aris::control::Controller*>(target.master);
		// ��һ��count��ȡ��������ĵ�ǰλ��
		if (target.count == 1)
		{
			for (int i = 0; i < param.axis_active_vec.size(); i++)
			{
				param.axis_begin_pos_vec[i] = controller->motionAtAbs(i).targetPos();
			}
		}
		int total_count{ 1 };
		double p, v, a;
		int t_count;
		//���Թ켣//
		if (param.abs)
		{
			for (int i = 0; i < param.axis_active_vec.size(); i++)
			{
				//���ι켣�滮����moveAbsolute���������(target.count, param.axis_begin_pos_vec[i], param.axis_pos_vec[i], param.axis_vel_vec[i] / 1000, param.axis_acc_vec[i] / 1000 / 1000, param.axis_dec_vec[i] / 1000 / 1000)����ȡ����ֵ
				//Ȼ��滮����ǰcount��ָ��λ��p��ָ���ٶ�v��ָ����ٶ�a���Լ������ι켣����count��t_count
				aris::plan::moveAbsolute(target.count, param.axis_begin_pos_vec[i], param.axis_pos_vec[i], param.axis_vel_vec[i] / 1000, param.axis_acc_vec[i] / 1000 / 1000, param.axis_dec_vec[i] / 1000 / 1000, p, v, a, t_count);
				controller->motionAtAbs(i).setTargetPos(p);
				total_count = std::max(total_count, t_count);
			}
		}
		//��Թ켣//
		else
		{
			for (int i = 0; i < param.axis_active_vec.size(); i++)
			{
				aris::plan::moveAbsolute(target.count, param.axis_begin_pos_vec[i], param.axis_begin_pos_vec[i] + param.axis_pos_vec[i], param.axis_vel_vec[i] / 1000, param.axis_acc_vec[i] / 1000 / 1000, param.axis_dec_vec[i] / 1000 / 1000, p, v, a, t_count);
				controller->motionAtAbs(i).setTargetPos(p);
				total_count = std::max(total_count, t_count);
			}	
		}

		// ÿ1000ms��ӡ Ŀ��λ�á�ʵ��λ�á�ʵ���ٶȡ�ʵ�ʵ��� //
		auto &cout = controller->mout();
		if (target.count % 1000 == 0)
		{
			for (int i = 0; i < param.axis_active_vec.size(); i++)
			{
				cout << i <<":  "<< controller->motionAtAbs(i).targetPos() << "  " << controller->motionAtAbs(i).actualPos() << "  " << controller->motionAtAbs(i).actualVel() << "  " << controller->motionAtAbs(i).actualCur() << std::endl;
			}
		}
		// log Ŀ��λ�á�ʵ��λ�á�ʵ���ٶȡ�ʵ�ʵ��� //
		
		auto &lout = controller->lout();
		for (int i = 0; i < param.axis_active_vec.size(); i++)
		{
			lout << controller->motionAtAbs(i).targetPos() << "  " << controller->motionAtAbs(i).actualPos() << "  " << controller->motionAtAbs(i).actualVel() << "  " << controller->motionAtAbs(i).actualCur() << "  ";
		}
		lout << std::endl;
		// ����total_count - target.count��arisʵʱ�ˣ�ֵΪ-1������ֵΪ0��������ֵ����0������ִ����һ��count
		return total_count - target.count;
	}

	auto virtual collectNrt(PlanTarget &target)->void {}

	explicit MoveDELTA(const std::string &name = "MoveDELTA_plan") :Plan(name), fore_vel(FORE_VEL_LENGTH + 1), tempforce(0)
	{
		command().loadXmlStr(
			"<moveEAP default_child_type=\"Param\">"
			"	<group type=\"GroupParam\" default_child_type=\"Param\">"
			"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"motion_id\">"
			"			<all/>"
			"			<motion_id default=\"0\"/>"
			"		</unique>"
			"		<pos default=\"6.28\"/>"
			"		<vel default=\"0.5\"/>"
			"		<acc default=\"1\"/>"
			"		<dec default=\"1\"/>"
			"		<abs default=\"1\"/>"
			"	</group>"
			"</moveEAP>");
	}
};

// ���켣MoveDELTA��ӵ��켣�滮��planPool��
auto createPlanRootRokaeXB4()->std::unique_ptr<aris::plan::PlanRoot>
{
	std::unique_ptr<aris::plan::PlanRoot> plan_root(new aris::plan::PlanRoot);

	plan_root->planPool().add<aris::plan::EnablePlan>();
	plan_root->planPool().add<aris::plan::DisablePlan>();
	plan_root->planPool().add<aris::plan::ModePlan>();
	plan_root->planPool().add<aris::plan::RecoverPlan>();
	plan_root->planPool().add<aris::plan::SleepPlan>();
	auto &rs = plan_root->planPool().add<aris::plan::ResetPlan>();
	rs.command().findByName("group")->findByName("pos")->loadXmlStr("<pos default=\"{0.0,0.0}\" abbreviation=\"p\"/>");

	plan_root->planPool().add<aris::plan::MovePlan>();
	plan_root->planPool().add<aris::plan::MoveJ>();
	plan_root->planPool().add<MoveDELTA>();
	plan_root->planPool().add<aris::plan::Show>();

	return plan_root;
}

// ������
int main(int argc, char *argv[])
{
	//����Ethercat��վ����
    aris::control::EthercatMaster mst;
	//�Զ�ɨ�裬���Ӵ�վ
    mst.scan();
    std::cout<<mst.xmlString()<<std::endl;

	//cs�����Ա���������ã�aris��ͷ�ļ���server�������ռ䣬ControlServer�ǽṹ��
    auto&cs = aris::server::ControlServer::instance();
    cs.resetController(createControllerRokaeXB4().release());
    cs.resetPlanRoot(createPlanRootRokaeXB4().release());

    std::cout<<"start thread"<<std::endl;
	//�����߳�
	cs.start();
	//getline�ǽ������ֵ��ֵ��command_in
	for (std::string command_in; std::getline(std::cin, command_in);)
	{
		try
		{
			auto id = cs.executeCmd(aris::core::Msg(command_in));
			std::cout << "command id:" << id << std::endl;
		}
		catch (std::exception &e)
		{
			std::cout << e.what() << std::endl;
			LOG_ERROR << e.what() << std::endl;
		}
	}
}
