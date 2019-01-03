#include"forcecontrol.h"
#include <atomic>


using namespace aris::dynamic;
using namespace aris::plan;

namespace forcecontrol
{
	// �����϶��������ؽڻ���6���켣����˶��켣--���뵥���ؽڣ��Ƕ�λ�ã��ؽڰ��������ٶȹ켣ִ�У��ٶ�ǰ������������ //
	struct MoveJRCParam
	{
		double kp_p, kp_v, ki_v;
		double vel, acc, dec;
		std::vector<double> joint_pos_vec, begin_joint_pos_vec;
		std::vector<bool> joint_active_vec;
	};
	static std::atomic_bool enable_moveJRC = true;
	auto MoveJRC::prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
	{
		auto c = dynamic_cast<aris::control::Controller*>(target.master);
		MoveJRCParam param;
		enable_moveJRC = true;
		for (auto cmd_param : params)
		{
			if (cmd_param.first == "all")
			{
				param.joint_active_vec.resize(target.model->motionPool().size(), true);
			}
			else if (cmd_param.first == "none")
			{
				param.joint_active_vec.resize(target.model->motionPool().size(), false);
			}
			else if (cmd_param.first == "motion_id")
			{
				param.joint_active_vec.resize(target.model->motionPool().size(), false);
				param.joint_active_vec.at(std::stoi(cmd_param.second)) = true;
			}
			else if (cmd_param.first == "physical_id")
			{
				param.joint_active_vec.resize(c->motionPool().size(), false);
				param.joint_active_vec.at(c->motionAtPhy(std::stoi(cmd_param.second)).phyId()) = true;
			}
			else if (cmd_param.first == "slave_id")
			{
				param.joint_active_vec.resize(c->motionPool().size(), false);
				param.joint_active_vec.at(c->motionAtPhy(std::stoi(cmd_param.second)).slaId()) = true;
			}
			else if (cmd_param.first == "pos")
			{
				aris::core::Matrix mat = target.model->calculator().calculateExpression(cmd_param.second);
				if (mat.size() == 1)param.joint_pos_vec.resize(c->motionPool().size(), mat.toDouble());
				else
				{
					param.joint_pos_vec.resize(mat.size());
					std::copy(mat.begin(), mat.end(), param.joint_pos_vec.begin());
				}
			}
			else if (cmd_param.first == "vel")
			{
				param.vel = std::stod(cmd_param.second);
			}
			else if (cmd_param.first == "acc")
			{
				param.acc = std::stod(cmd_param.second);
			}
			else if (cmd_param.first == "dec")
			{
				param.dec = std::stod(cmd_param.second);
			}
			else if (cmd_param.first == "kp_p")
			{
				param.kp_p = std::stod(cmd_param.second);
			}
			else if (cmd_param.first == "kp_v")
			{
				param.kp_v = std::stod(cmd_param.second);
			}
			else if (cmd_param.first == "ki_v")
			{
				param.ki_v = std::stod(cmd_param.second);
			}
		}

		param.begin_joint_pos_vec.resize(target.model->motionPool().size());

		target.param = param;

		target.option |=
			Plan::USE_TARGET_POS |
			Plan::USE_VEL_OFFSET |
			//#ifdef WIN32
			Plan::NOT_CHECK_POS_MIN |
			Plan::NOT_CHECK_POS_MAX |
			Plan::NOT_CHECK_POS_CONTINUOUS |
			Plan::NOT_CHECK_POS_CONTINUOUS_AT_START |
			Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER |
			Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER_AT_START |
			Plan::NOT_CHECK_POS_FOLLOWING_ERROR |
			//#endif
			Plan::NOT_CHECK_VEL_MIN |
			Plan::NOT_CHECK_VEL_MAX |
			Plan::NOT_CHECK_VEL_CONTINUOUS |
			Plan::NOT_CHECK_VEL_CONTINUOUS_AT_START |
			Plan::NOT_CHECK_VEL_FOLLOWING_ERROR;

	}
	auto MoveJRC::executeRT(PlanTarget &target)->int
	{
		auto &param = std::any_cast<MoveJRCParam&>(target.param);
		auto controller = dynamic_cast<aris::control::Controller *>(target.master);
		static bool is_running{ true };
		static double vinteg[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		bool ds_is_all_finished{ true };
		bool md_is_all_finished{ true };

		//��һ�����ڣ���Ŀ�����Ŀ���ģʽ�л�����������ģʽ
		if (target.count == 1)
		{

			is_running = true;
			for (Size i = 0; i < param.joint_active_vec.size(); ++i)
			{
				if (param.joint_active_vec[i])
				{
					param.begin_joint_pos_vec[i] = target.model->motionPool()[i].mp();
					controller->motionPool().at(i).setModeOfOperation(10);	//�л�����������
				}
			}
		}

		//���һ�����ڽ�Ŀ����ȥʹ��
		if (!enable_moveJRC)
		{
			is_running = false;
		}
		if (!is_running)
		{
			for (Size i = 0; i < param.joint_active_vec.size(); ++i)
			{
				if (param.joint_active_vec[i])
				{
					//controller->motionPool().at(i).setModeOfOperation(8);
					//controller->motionPool().at(i).setTargetPos(controller->motionAtAbs(i).actualPos());
					//target.model->motionPool().at(i).setMp(controller->motionAtAbs(i).actualPos());
					auto ret = controller->motionPool().at(i).disable();
					if (ret)
					{
						ds_is_all_finished = false;
					}
				}
			}
		}

		//��Ŀ�����ɵ���ģʽ�л���λ��ģʽ
		if (!is_running&&ds_is_all_finished)
		{
			for (Size i = 0; i < param.joint_active_vec.size(); ++i)
			{
				if (param.joint_active_vec[i])
				{
					controller->motionPool().at(i).setModeOfOperation(8);
					auto ret = controller->motionPool().at(i).mode(8);
					if (ret)
					{
						md_is_all_finished = false;
					}
				}
			}
		}

		//����ѧ
		for (int i = 0; i < 6; ++i)
		{
			target.model->motionPool()[i].setMp(controller->motionPool()[i].actualPos());
			target.model->motionPool().at(i).setMv(controller->motionAtAbs(i).actualVel());
			target.model->motionPool().at(i).setMa(0.0);
		}

		target.model->solverPool()[1].kinPos();
		target.model->solverPool()[1].kinVel();
		target.model->solverPool()[2].dynAccAndFce();

		if (is_running)
		{
			for (Size i = 0; i < param.joint_active_vec.size(); ++i)
			{
				if (param.joint_active_vec[i])
				{
					double p, v, pa, vt, va, voff, ft, foff, ft_offset;
					p = controller->motionAtAbs(i).actualPos();
					v = 0.0;
					pa = controller->motionAtAbs(i).actualPos();
					va = controller->motionAtAbs(i).actualVel();
					voff = v * 1000;
					foff = 0.0;
					vt = param.kp_p*(p - pa) + voff;
					//�����ٶȵķ�Χ��-1.0~1.0֮��
					vt = std::max(-1.0, vt);
					vt = std::min(1.0, vt);

					vinteg[i] = vinteg[i] + vt - va;
					ft = param.kp_v*(vt - va) + param.ki_v*vinteg[i] + foff;
					//���Ƶ����ķ�Χ��-400~400(ǧ�������������1000)֮��
					ft = std::max(-400.0, ft);
					ft = std::min(400.0, ft);

					//�϶�ʾ��
					//constexpr double f_static[6] = { 9.349947583,11.64080253,4.770140543,3.631416685,2.58310847,1.783739862 };
					//constexpr double f_vel[6] = { 7.80825641,13.26518528,7.856443575,3.354615249,1.419632126,0.319206404 };
					//constexpr double f_acc[6] = { 0,3.555679326,0.344454603,0.148247716,0.048552673,0.033815455 };
					//constexpr double f2c_index[6] = { 9.07327526291993, 9.07327526291993, 17.5690184835913, 39.0310903520972, 66.3992503259041, 107.566785527965 };
					//constexpr double max_static_vel[6] = { 0.1, 0.1, 0.1, 0.05, 0.05, 0.075 };
					//constexpr double f_static_index[6] = { 0.5, 0.5, 0.5, 0.85, 0.95, 0.8 };

					auto real_vel = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
					ft_offset = (f_vel[i] * controller->motionAtAbs(i).actualVel() + f_static_index[i] * f_static[i] * real_vel / max_static_vel[i])*f2c_index[i];

					ft_offset = std::max(-500.0, ft_offset);
					ft_offset = std::min(500.0, ft_offset);

					controller->motionAtAbs(i).setTargetCur(ft_offset + target.model->motionPool()[i].mfDyn()*f2c_index[i]);

					//��ӡPID���ƽ��
					auto &cout = controller->mout();
					if (target.count % 100 == 0)
					{
						//cout << "ft:" << ft << "  " << "vt:" << vt << "  " << "va:" << va << "  " << "param.kp_v*(vt - va):" << param.kp_v*(vt - va) << "  " << "param.ki_v*vinteg[i]:" << param.ki_v*vinteg[i] << "    ";
						cout << "feedbackf:" << std::setw(10) << controller->motionAtAbs(i).actualCur()
							<< "f:" << std::setw(10) << ft_offset
							<< "p:" << std::setw(10) << p
							<< "pa:" << std::setw(10) << pa
							<< "va:" << std::setw(10) << va << std::endl;
					}
				}
			}
		}

		if (!target.model->solverPool().at(1).kinPos())return -1;

		// ��ӡ���� //
		/*
		auto &cout = controller->mout();
		if (target.count % 100 == 0)
		{
			for (Size i = 0; i < param.joint_active_vec.size(); i++)
			{
				if (param.joint_active_vec[i])
				{
					cout << "target_cur" << i + 1 << ":" << controller->motionAtAbs(i).targetCur() << "  ";
					cout << "pos" << i + 1 << ":" << controller->motionAtAbs(i).actualPos() << "  ";
					cout << "vel" << i + 1 << ":" << controller->motionAtAbs(i).actualVel() << "  ";
					cout << "cur" << i + 1 << ":" << controller->motionAtAbs(i).actualCur() << "  ";
				}
			}
			cout << std::endl;
		}
		*/

		// log ���� //
		auto &lout = controller->lout();
		for (Size i = 0; i < param.joint_active_vec.size(); i++)
		{
			lout << std::setw(10) << controller->motionAtAbs(i).targetCur() << ",";
			lout << std::setw(10) << controller->motionAtAbs(i).actualPos() << ",";
			lout << std::setw(10) << controller->motionAtAbs(i).actualVel() << ",";
			lout << std::setw(10) << controller->motionAtAbs(i).actualCur() << " | ";
		}
		lout << std::endl;

		return (!is_running&&ds_is_all_finished&&md_is_all_finished) ? 0 : 1;
	}
	auto MoveJRC::collectNrt(PlanTarget &target)->void {}
	MoveJRC::MoveJRC(const std::string &name) :Plan(name)
	{
		command().loadXmlStr(
			"<moveJRC default_child_type=\"Param\">"
			"	<group type=\"GroupParam\" default_child_type=\"Param\">"
			"		<limit_time default=\"5000\"/>"
			"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"all\">"
			"			<all abbreviation=\"a\"/>"
			"			<motion_id abbreviation=\"m\" default=\"0\"/>"
			"			<physical_id abbreviation=\"p\" default=\"0\"/>"
			"			<slave_id abbreviation=\"s\" default=\"0\"/>"
			"		</unique>"
			"		<pos default=\"0\"/>"
			"		<vel default=\"0.5\"/>"
			"		<acc default=\"1\"/>"
			"		<dec default=\"1\"/>"
			"		<kp_p default=\"1\"/>"
			"		<kp_v default=\"100\"/>"
			"		<ki_v default=\"0.1\"/>"
			"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_all\">"
			"			<check_all/>"
			"			<check_none/>"
			"			<group type=\"GroupParam\" default_child_type=\"Param\">"
			"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos\">"
			"					<check_pos/>"
			"					<not_check_pos/>"
			"					<group type=\"GroupParam\" default_child_type=\"Param\">"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_max\">"
			"							<check_pos_max/>"
			"							<not_check_pos_max/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_min\">"
			"							<check_pos_min/>"
			"							<not_check_pos_min/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous\">"
			"							<check_pos_continuous/>"
			"							<not_check_pos_continuous/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_at_start\">"
			"							<check_pos_continuous_at_start/>"
			"							<not_check_pos_continuous_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order\">"
			"							<check_pos_continuous_second_order/>"
			"							<not_check_pos_continuous_second_order/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order_at_start\">"
			"							<check_pos_continuous_second_order_at_start/>"
			"							<not_check_pos_continuous_second_order_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_following_error\">"
			"							<check_pos_following_error/>"
			"							<not_check_pos_following_error />"
			"						</unique>"
			"					</group>"
			"				</unique>"
			"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel\">"
			"					<check_vel/>"
			"					<not_check_vel/>"
			"					<group type=\"GroupParam\" default_child_type=\"Param\">"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_max\">"
			"							<check_vel_max/>"
			"							<not_check_vel_max/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_min\">"
			"							<check_vel_min/>"
			"							<not_check_vel_min/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous\">"
			"							<check_vel_continuous/>"
			"							<not_check_vel_continuous/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous_at_start\">"
			"							<check_vel_continuous_at_start/>"
			"							<not_check_vel_continuous_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_following_error\">"
			"							<check_vel_following_error/>"
			"							<not_check_vel_following_error />"
			"						</unique>"
			"					</group>"
			"				</unique>"
			"			</group>"
			"		</unique>"
			"	</group>"
			"</moveJRC>");
	}


	// ����ĩ�˿ռ䡪������ĩ��pq��̬����ĩ��PID�㷨����ĩ�������⵽��ռ䣬Ȼ�����ÿ����������ŵ����ܹ�����ĩ�������ٶ�ǰ������������ //
	struct MovePQCrashParam
	{
		std::vector<double> kp_p;
		std::vector<double> kp_v;
		std::vector<double> ki_v;

		std::vector<double> pqt;
		std::vector<double> pqa;
		std::vector<double> vt;

		std::vector<double> ft;
		std::vector<double> ft_input;
	};
	static std::atomic_bool enable_movePQCrash = true;
	auto MovePQCrash::prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
		{
			auto c = dynamic_cast<aris::control::Controller*>(target.master);
			MovePQCrashParam param;
			enable_movePQCrash = true;
			param.kp_p.resize(7, 0.0);
			param.kp_v.resize(6, 0.0);
			param.ki_v.resize(6, 0.0);

			param.pqt.resize(7, 0.0);
			param.pqa.resize(7, 0.0);
			param.vt.resize(7, 0.0);
			param.ft.resize(6, 0.0);
			param.ft_input.resize(6, 0.0);
			//params.at("pqt")
			for (auto &p : params)
			{
				if (p.first == "pqt")
				{
					auto pqarray = target.model->calculator().calculateExpression(p.second);
					param.pqt.assign(pqarray.begin(), pqarray.end());
				}
				else if (p.first == "kp_p")
				{
					auto v = target.model->calculator().calculateExpression(p.second);
					if (v.size() == 1)
					{
						std::fill(param.kp_p.begin(), param.kp_p.end(), v.toDouble());
					}
					else if (v.size() == param.kp_p.size())
					{
						param.kp_p.assign(v.begin(), v.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.kp_p.size(); ++i)
					{
						if (param.kp_p[i] > 10000.0)
						{
							param.kp_p[i] = 10000.0;
						}
						if (param.kp_p[i] < 0.01)
						{
							param.kp_p[i] = 0.01;
						}
					}
				}
				else if (p.first == "kp_v")
				{
					auto a = target.model->calculator().calculateExpression(p.second);
					if (a.size() == 1)
					{
						std::fill(param.kp_v.begin(), param.kp_v.end(), a.toDouble());
					}
					else if (a.size() == param.kp_v.size())
					{
						param.kp_v.assign(a.begin(), a.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.kp_v.size(); ++i)
					{
						if (param.kp_v[i] > 10000.0)
						{
							param.kp_v[i] = 10000.0;
						}
						if (param.kp_v[i] < 0.01)
						{
							param.kp_v[i] = 0.01;
						}
					}
				}
				else if (p.first == "ki_v")
				{
					auto d = target.model->calculator().calculateExpression(p.second);
					if (d.size() == 1)
					{
						std::fill(param.ki_v.begin(), param.ki_v.end(), d.toDouble());
					}
					else if (d.size() == param.ki_v.size())
					{
						param.ki_v.assign(d.begin(), d.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.ki_v.size(); ++i)
					{
						if (param.ki_v[i] > 10000.0)
						{
							param.ki_v[i] = 10000.0;
						}
						if (param.ki_v[i] < 0.001)
						{
							param.ki_v[i] = 0.001;
						}
					}
				}

			}
			target.param = param;

			target.option |=
				Plan::USE_VEL_OFFSET |
				//#ifdef WIN32
				Plan::NOT_CHECK_POS_MIN |
				Plan::NOT_CHECK_POS_MAX |
				Plan::NOT_CHECK_POS_CONTINUOUS |
				Plan::NOT_CHECK_POS_CONTINUOUS_AT_START |
				Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER |
				Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER_AT_START |
				Plan::NOT_CHECK_POS_FOLLOWING_ERROR |
				//#endif
				Plan::NOT_CHECK_VEL_MIN |
				Plan::NOT_CHECK_VEL_MAX |
				Plan::NOT_CHECK_VEL_CONTINUOUS |
				Plan::NOT_CHECK_VEL_CONTINUOUS_AT_START |
				Plan::NOT_CHECK_VEL_FOLLOWING_ERROR;

		}
	auto MovePQCrash::executeRT(PlanTarget &target)->int
		{
			auto &param = std::any_cast<MovePQCrashParam&>(target.param);
			auto controller = dynamic_cast<aris::control::Controller *>(target.master);
			static double va[7];
			static bool is_running{ true };
			static double vinteg[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			static double vproportion[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			bool ds_is_all_finished{ true };
			bool md_is_all_finished{ true };

			//��һ�����ڣ���Ŀ�����Ŀ���ģʽ�л�����������ģʽ
			if (target.count == 1)
			{
				is_running = true;

				for (Size i = 0; i < param.ft.size(); ++i)
				{
					controller->motionPool().at(i).setModeOfOperation(10);	//�л�����������
				}
			}

			//���һ�����ڽ�Ŀ����ȥʹ��
			if (!enable_movePQCrash)
			{
				is_running = false;
			}
			if (!is_running)
			{
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					auto ret = controller->motionPool().at(i).disable();
					if (ret)
					{
						ds_is_all_finished = false;
					}
				}
			}

			//��Ŀ�����ɵ���ģʽ�л���λ��ģʽ
			if (!is_running&&ds_is_all_finished)
			{
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					controller->motionPool().at(i).setModeOfOperation(8);
					//auto ret = controller->motionPool().at(i).modeOfDisplay();
					auto ret = controller->motionPool().at(i).mode(8);
					if (ret)
					{
						md_is_all_finished = false;
					}
				}
			}

			//�ٶȵ�����
			for (int i = 0; i < param.ft.size(); ++i)
			{
				target.model->motionPool()[i].setMp(controller->motionPool()[i].actualPos());
				target.model->motionPool().at(i).setMv(controller->motionAtAbs(i).actualVel());
				target.model->motionPool().at(i).setMa(0.0);
			}

			target.model->solverPool()[1].kinPos();
			target.model->solverPool()[1].kinVel();
			double ee_acc[6]{ 0,0,0,0,0,0 };
			target.model->generalMotionPool()[0].setMaa(ee_acc);
			target.model->solverPool()[0].dynAccAndFce();
			target.model->solverPool()[2].dynAccAndFce();

			double ft_friction[6];
			double ft_offset[6];
			double real_vel[6];
			double ft_friction1[6], ft_friction2[6], ft_dynamic[6], ft_pid[6];
			static double ft_friction2_index[6] = { 3.0, 3.0, 3.0, 3.0, 3.0, 3.0 };
			if (is_running)
			{
				//λ�û�PID+�ٶ�����
				target.model->generalMotionPool().at(0).getMpq(param.pqa.data());

				for (Size i = 0; i < param.kp_p.size(); ++i)
				{
					param.vt[i] = param.kp_p[i] * (param.pqt[i] - param.pqa[i]);
					param.vt[i] = std::max(std::min(param.vt[i], vt_limit[i]), -vt_limit[i]);
				}

				//�ٶȻ�PID+�������ص�����
				target.model->generalMotionPool().at(0).getMvq(va);

				for (Size i = 0; i < param.ft.size(); ++i)
				{
					double limit = mt_limit[i];

					//vproportion[i] = std::min(r2*limit, std::max(-r2*limit, param.kp_v[i] * (param.vt[i] - va[i])));
					vproportion[i] = param.kp_v[i] * (param.vt[i] - va[i]);
					double vinteg_limit = std::max(0.0, limit - vproportion[i]);
					vinteg[i] = std::min(vinteg_limit, std::max(-vinteg_limit, vinteg[i] + param.ki_v[i] * (param.vt[i] - va[i])));

					param.ft[i] = vproportion[i] + vinteg[i];
					//��������
					//if (i < 3)
					//{
					//	param.ft[i] = std::max(std::min(param.ft[i], ft_limit), -ft_limit);
					//}
					//���ص�����
					//else
					//{
					//	param.ft[i] = std::max(std::min(param.ft[i], Mt_limit), -Mt_limit);
					//}
				}

				s_c3a(param.pqa.data(), param.ft.data(), param.ft.data() + 3);

				//ͨ���ſ˱Ⱦ���param.ftת�����ؽ�param.ft_input
				auto &fwd = dynamic_cast<aris::dynamic::ForwardKinematicSolver&>(target.model->solverPool()[1]);

				fwd.cptJacobi();
				/*		double U[36], tau[6], tau2[6], J_fce[36];
						Size p[6], rank;

						s_householder_utp(6, 6, inv.Jf(), U, tau, p, rank);
						s_householder_utp2pinv(6, 6, rank, U, tau, p, J_fce, tau2);*/
				s_mm(6, 1, 6, fwd.Jf(), aris::dynamic::ColMajor{ 6 }, param.ft.data(), 1, param.ft_input.data(), 1);

				//����ѧ�غ�
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					//double ft_friction1, ft_friction2, ft_dynamic, ft_pid;

					//����ѧ����
					//constexpr double f_static[6] = { 9.349947583,11.64080253,4.770140543,3.631416685,2.58310847,1.783739862 };
					//constexpr double f_vel[6] = { 7.80825641,13.26518528,7.856443575,3.354615249,1.419632126,0.319206404 };
					//constexpr double f_acc[6] = { 0,3.555679326,0.344454603,0.148247716,0.048552673,0.033815455 };
					//constexpr double f2c_index[6] = { 9.07327526291993, 9.07327526291993, 17.5690184835913, 39.0310903520972, 66.3992503259041, 107.566785527965 };
					//constexpr double f_static_index[6] = {0.5, 0.5, 0.5, 0.85, 0.95, 0.8};

					//��Ħ����+��Ħ����=ft_friction

					real_vel[i] = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
					ft_friction1[i] = 0.8*(f_static[i] * real_vel[i] / max_static_vel[i]);

					double ft_friction2_max = std::max(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? f_static[i] - ft_friction1[i] : f_static[i] + ft_friction1[i]);
					double ft_friction2_min = std::min(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? -f_static[i] + ft_friction1[i] : -f_static[i] - ft_friction1[i]);

					ft_friction2[i] = std::max(ft_friction2_min, std::min(ft_friction2_max, ft_friction2_index[i] * param.ft_input[i]));

					ft_friction[i] = ft_friction1[i] + ft_friction2[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

					//auto real_vel = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
					//ft_friction = (f_vel[i] * controller->motionAtAbs(i).actualVel() + f_static_index[i] * f_static[i] * real_vel / max_static_vel[i])*f2c_index[i];

					ft_friction[i] = std::max(-500.0, ft_friction[i]);
					ft_friction[i] = std::min(500.0, ft_friction[i]);

					//����ѧ�غ�=ft_dynamic
					ft_dynamic[i] = target.model->motionPool()[i].mfDyn();

					//PID����=ft_pid
					ft_pid[i] = param.ft_input[i];
					//ft_pid = 0.0;

					ft_offset[i] = (ft_friction[i] + ft_dynamic[i] + ft_pid[i])*f2c_index[i];
					controller->motionAtAbs(i).setTargetCur(ft_offset[i]);
				}
			}

			//��ӡ//
			auto &cout = controller->mout();
			if (target.count % 1000 == 0)
			{
				cout << "friction1 and friction2:";
				for (Size i = 0; i < 6; i++)
				{
					cout << ft_friction1[i] << "  ";
					cout << ft_friction2[i] << "  ";
				}
				cout << std::endl;
				cout << "vt:";
				for (Size i = 0; i < 6; i++)
				{
					cout << param.vt[i] << "  ";
				}
				cout << std::endl;

				cout << "ft:";
				for (Size i = 0; i < 6; i++)
				{
					cout << param.ft[i] << "  ";
				}
				cout << std::endl;

				cout << "fi:";
				for (Size i = 0; i < 6; i++)
				{
					cout << param.ft_input[i] * f2c_index[i] << "  ";
				}
				cout << std::endl;


				cout << "------------------------------------------------" << std::endl;

				/*
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(6) << "pos" << i + 1 << ":" << controller->motionAtAbs(i).actualPos();
					cout << std::setw(6) << "vel" << i + 1 << ":" << controller->motionAtAbs(i).actualVel();
					cout << std::setw(6) << "cur" << i + 1 << ":" << controller->motionAtAbs(i).actualCur();
				}
				cout << std::endl;*/
			}

			// log //
			auto &lout = controller->lout();
			for (Size i = 0; i < param.kp_p.size(); i++)
			{
				lout << param.kp_p[i] << ",";
				lout << param.kp_v[i] << ",";
				lout << param.ki_v[i] << ",";
				lout << param.pqt[i] << ",";
				lout << param.pqa[i] << ",";
				lout << param.vt[i] << ",";
				lout << va[i] << ",";
			}
			for (Size i = 0; i < param.ft.size(); i++)
			{
				lout << vproportion[i] << ",";
				lout << vinteg[i] << ",";
				lout << param.ft[i] << ",";
				lout << param.ft_input[i] << ",";
				lout << ft_friction1[i] << ",";
				lout << ft_friction2[i] << ",";
				lout << ft_friction[i] << ",";
				lout << ft_dynamic[i] << ",";
				lout << ft_offset[i] << ",";
				lout << ft_pid[i] << ",";
				lout << controller->motionAtAbs(i).targetCur() << ",";
				lout << controller->motionAtAbs(i).actualPos() << ",";
				lout << controller->motionAtAbs(i).actualVel() << ",";
				lout << controller->motionAtAbs(i).actualCur();
			}
			lout << std::endl;

			return (!is_running&&ds_is_all_finished&&md_is_all_finished) ? 0 : 1;
		}
	auto MovePQCrash::collectNrt(PlanTarget &target)->void {}
	MovePQCrash::MovePQCrash(const std::string &name) :Plan(name)
		{
			command().loadXmlStr(
				"<movePQCrash>"
				"	<group type=\"GroupParam\" default_child_type=\"Param\">"
				"		<pqt default=\"{0.42,0.0,0.55,0,0.7071,0,0.7071}\" abbreviation=\"p\"/>"
				"		<kp_p default=\"{1.0,1.0,1.0,1.0,1.0,1.0,1.0}\"/>"
				"		<kp_v default=\"0.1*{100,100,100,100,100,100}\"/>"
				"		<ki_v default=\"30*{1,1,1,1,1,1}\"/>"
				"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_all\">"
				"			<check_all/>"
				"			<check_none/>"
				"			<group type=\"GroupParam\" default_child_type=\"Param\">"
				"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos\">"
				"					<check_pos/>"
				"					<not_check_pos/>"
				"					<group type=\"GroupParam\" default_child_type=\"Param\">"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_max\">"
				"							<check_pos_max/>"
				"							<not_check_pos_max/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_min\">"
				"							<check_pos_min/>"
				"							<not_check_pos_min/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous\">"
				"							<check_pos_continuous/>"
				"							<not_check_pos_continuous/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_at_start\">"
				"							<check_pos_continuous_at_start/>"
				"							<not_check_pos_continuous_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order\">"
				"							<check_pos_continuous_second_order/>"
				"							<not_check_pos_continuous_second_order/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order_at_start\">"
				"							<check_pos_continuous_second_order_at_start/>"
				"							<not_check_pos_continuous_second_order_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_following_error\">"
				"							<check_pos_following_error/>"
				"							<not_check_pos_following_error />"
				"						</unique>"
				"					</group>"
				"				</unique>"
				"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel\">"
				"					<check_vel/>"
				"					<not_check_vel/>"
				"					<group type=\"GroupParam\" default_child_type=\"Param\">"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_max\">"
				"							<check_vel_max/>"
				"							<not_check_vel_max/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_min\">"
				"							<check_vel_min/>"
				"							<not_check_vel_min/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous\">"
				"							<check_vel_continuous/>"
				"							<not_check_vel_continuous/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous_at_start\">"
				"							<check_vel_continuous_at_start/>"
				"							<not_check_vel_continuous_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_following_error\">"
				"							<check_vel_following_error/>"
				"							<not_check_vel_following_error />"
				"						</unique>"
				"					</group>"
				"				</unique>"
				"			</group>"
				"		</unique>"
				"	</group>"
				"</movePQCrash>");
		}


	// ������ռ䡪������ĩ��pq��̬���ȶ�ĩ��pq�������ι켣�滮��Ȼ��滮�õ�ĩ��pq���⵽��ռ䣬���ͨ����ռ�PID�㷨����ÿ�����������ʵ�֣�����С���ٶ�ǰ������������ //
	struct MoveJCrashParam
	{
		std::vector<double> kp_p;
		std::vector<double> kp_v;
		std::vector<double> ki_v;

		std::vector<double> pqt;
		std::vector<double> pqb;
		std::vector<double> vqt;
		std::vector<double> xyz;
		double vel, acc, dec;
		int which_dir;
		std::vector<double> pt;
		std::vector<double> pa;
		std::vector<double> vt;
		std::vector<double> va;
		std::vector<double> vfwd;

		std::vector<double> ft;
	};
	static std::atomic_bool enable_moveJCrash = true;
	auto MoveJCrash::prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
		{
			auto c = dynamic_cast<aris::control::Controller*>(target.master);
			MoveJCrashParam param;
			enable_moveJCrash = true;
			param.kp_p.resize(6, 0.0);
			param.kp_v.resize(6, 0.0);
			param.ki_v.resize(6, 0.0);

			param.pqt.resize(7, 0.0);
			param.pqb.resize(7, 0.0);
			param.vqt.resize(7, 0.0);
			param.xyz.resize(3, 0.0);
			param.which_dir = 0;
			param.pt.resize(6, 0.0);
			param.pa.resize(6, 0.0);
			param.vt.resize(6, 0.0);
			param.va.resize(6, 0.0);
			param.vfwd.resize(6, 0.0);
			param.ft.resize(6, 0.0);
			//params.at("pqt")
			for (auto &p : params)
			{
				if (p.first == "pqt")
				{
					auto pqarray = target.model->calculator().calculateExpression(p.second);
					param.pqt.assign(pqarray.begin(), pqarray.end());
					param.which_dir = 0;
				}
				else if (p.first == "xyz")
				{
					auto xyzarray = target.model->calculator().calculateExpression(p.second);
					param.xyz.assign(xyzarray.begin(), xyzarray.end());
					param.which_dir = 1;
				}
				else if (p.first == "vel")
				{
					param.vel = std::stod(p.second);
				}
				else if (p.first == "acc")
				{
					param.acc = std::stod(p.second);
				}
				else if (p.first == "dec")
				{
					param.dec = std::stod(p.second);
				}
				else if (p.first == "kp_p")
				{
					auto v = target.model->calculator().calculateExpression(p.second);
					if (v.size() == 1)
					{
						std::fill(param.kp_p.begin(), param.kp_p.end(), v.toDouble());
					}
					else if (v.size() == param.kp_p.size())
					{
						param.kp_p.assign(v.begin(), v.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.kp_p.size(); ++i)
					{
						if (param.kp_p[i] > 10000.0)
						{
							param.kp_p[i] = 10000.0;
						}
						if (param.kp_p[i] < 0.01)
						{
							param.kp_p[i] = 0.01;
						}
					}
				}
				else if (p.first == "kp_v")
				{
					auto a = target.model->calculator().calculateExpression(p.second);
					if (a.size() == 1)
					{
						std::fill(param.kp_v.begin(), param.kp_v.end(), a.toDouble());
					}
					else if (a.size() == param.kp_v.size())
					{
						param.kp_v.assign(a.begin(), a.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.kp_v.size(); ++i)
					{
						if (param.kp_v[i] > 10000.0)
						{
							param.kp_v[i] = 10000.0;
						}
						if (param.kp_v[i] < 0.01)
						{
							param.kp_v[i] = 0.01;
						}
					}
				}
				else if (p.first == "ki_v")
				{
					auto d = target.model->calculator().calculateExpression(p.second);
					if (d.size() == 1)
					{
						std::fill(param.ki_v.begin(), param.ki_v.end(), d.toDouble());
					}
					else if (d.size() == param.ki_v.size())
					{
						param.ki_v.assign(d.begin(), d.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.ki_v.size(); ++i)
					{
						if (param.ki_v[i] > 10000.0)
						{
							param.ki_v[i] = 10000.0;
						}
						if (param.ki_v[i] < 0.001)
						{
							param.ki_v[i] = 0.001;
						}
					}
				}

			}
			target.param = param;

			target.option |=
				Plan::USE_VEL_OFFSET |
				//#ifdef WIN32
				Plan::NOT_CHECK_POS_MIN |
				Plan::NOT_CHECK_POS_MAX |
				Plan::NOT_CHECK_POS_CONTINUOUS |
				Plan::NOT_CHECK_POS_CONTINUOUS_AT_START |
				Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER |
				Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER_AT_START |
				Plan::NOT_CHECK_POS_FOLLOWING_ERROR |
				//#endif
				Plan::NOT_CHECK_VEL_MIN |
				Plan::NOT_CHECK_VEL_MAX |
				Plan::NOT_CHECK_VEL_CONTINUOUS |
				Plan::NOT_CHECK_VEL_CONTINUOUS_AT_START |
				Plan::NOT_CHECK_VEL_FOLLOWING_ERROR;

		}
	auto MoveJCrash::executeRT(PlanTarget &target)->int
		{
			auto &param = std::any_cast<MoveJCrashParam&>(target.param);
			auto controller = dynamic_cast<aris::control::Controller *>(target.master);
			static bool is_running{ true };
			static double vinteg[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			static double vproportion[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			bool ds_is_all_finished{ true };
			bool md_is_all_finished{ true };

			//��һ�����ڣ���Ŀ�����Ŀ���ģʽ�л�����������ģʽ		
			if (target.count == 1)
			{
				is_running = true;
				if (param.which_dir == 0)
				{
					target.model->generalMotionPool().at(0).getMpq(param.pqb.data());
				}
				else
				{
					target.model->generalMotionPool().at(0).getMpq(param.pqb.data());
					param.pqt.assign(param.pqb.begin(), param.pqb.end());
				}

				for (Size i = 0; i < param.ft.size(); ++i)
				{
					controller->motionPool().at(i).setModeOfOperation(10);	//�л�����������
				}
			}

			//���һ�����ڽ�Ŀ����ȥʹ��
			if (!enable_moveJCrash)
			{
				is_running = false;
			}
			if (!is_running)
			{
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					auto ret = controller->motionPool().at(i).disable();
					if (ret)
					{
						ds_is_all_finished = false;
					}
				}
			}

			//��Ŀ�����ɵ���ģʽ�л���λ��ģʽ
			if (!is_running&&ds_is_all_finished)
			{
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					controller->motionPool().at(i).setModeOfOperation(8);
					auto ret = controller->motionPool().at(i).mode(8);
					if (ret)
					{
						md_is_all_finished = false;
					}
				}
			}

			//�켣�滮
			aris::Size total_count{ 1 };
			if (param.which_dir == 0)
			{
				//��Ŀ��λ��pq���˶�ѧ���⣬��ȡ���ʵ��λ�á�ʵ���ٶ�
				target.model->generalMotionPool().at(0).setMpq(param.pqt.data());
				if (!target.model->solverPool().at(0).kinPos())return -1;
				for (Size i = 0; i < param.pt.size(); ++i)
				{
					param.pt[i] = target.model->motionPool().at(i).mp();		//motionPool()ָģ����������at(0)��ʾ��1��������
					param.pa[i] = controller->motionPool().at(i).actualPos();
					param.va[i] = controller->motionPool().at(i).actualVel();
				}
			}
			else
			{
				//x,y,z�������ι켣�滮
				double norm, p, v, a;
				norm = std::sqrt(param.xyz[0] * param.xyz[0] + param.xyz[1] * param.xyz[1] + param.xyz[2] * param.xyz[2]);
				aris::plan::moveAbsolute(target.count, 0.0, norm, param.vel / 1000, param.acc / 1000 / 1000, param.dec / 1000 / 1000, p, v, a, total_count);

				//double norm = aris::dynamic::s_norm(3, param.xyz.data());
				//aris::dynamic::s_vc(7, param.pqb.data(), param.pqt.data());
				//aris::dynamic::s_va(3, p / norm, param.xyz.data(), param.pqt.data());
				//aris::dynamic::s_vc(3, v / norm * 1000, param.xyz.data(), param.vqt.data());

				for (Size i = 0; i < param.xyz.size(); i++)
				{
					param.pqt[i] = param.pqb[i] + param.xyz[i] * p / norm;
					param.vqt[i] = param.xyz[i] * v / norm * 1000;
				}
				target.model->generalMotionPool().at(0).setMpq(param.pqt.data());
				if (!target.model->solverPool().at(0).kinPos())return -1;
				target.model->generalMotionPool().at(0).setMvq(param.vqt.data());
				target.model->solverPool().at(0).kinVel();

				for (Size i = 0; i < param.pt.size(); ++i)
				{
					param.pt[i] = target.model->motionPool().at(i).mp();		//motionPool()ָģ����������at(0)��ʾ��1��������
					param.vfwd[i] = target.model->motionPool().at(i).mv();
					param.pa[i] = controller->motionPool().at(i).actualPos();
					param.va[i] = controller->motionPool().at(i).actualVel();
				}
			}

			//ģ������
			for (int i = 0; i < param.ft.size(); ++i)
			{
				target.model->motionPool()[i].setMp(controller->motionPool()[i].actualPos());
				target.model->motionPool().at(i).setMv(controller->motionAtAbs(i).actualVel());
				target.model->motionPool().at(i).setMa(0.0);
			}
			target.model->solverPool()[1].kinPos();
			target.model->solverPool()[1].kinVel();
			target.model->solverPool()[2].dynAccAndFce();

			double ft_friction[6];
			double ft_offset[6];
			double real_vel[6];
			double ft_friction1[6], ft_friction2[6], ft_dynamic[6];
			static double ft_friction2_index[6] = { 5.0, 5.0, 5.0, 5.0, 5.0, 3.0 };
			if (is_running)
			{
				//λ�û�PID+�ٶ�����
				for (Size i = 0; i < param.kp_p.size(); ++i)
				{
					param.vt[i] = param.kp_p[i] * (param.pt[i] - param.pa[i]);
					param.vt[i] = std::max(std::min(param.vt[i], vt_limit[i]), -vt_limit[i]);
					param.vt[i] = param.vt[i] + param.vfwd[i];
				}

				//�ٶȻ�PID+�������ص�����
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					vproportion[i] = param.kp_v[i] * (param.vt[i] - param.va[i]);
					vinteg[i] = vinteg[i] + param.ki_v[i] * (param.vt[i] - param.va[i]);
					vinteg[i] = std::min(vinteg[i], fi_limit[i]);
					vinteg[i] = std::max(vinteg[i], -fi_limit[i]);

					param.ft[i] = vproportion[i] + vinteg[i];
					param.ft[i] = std::min(param.ft[i], ft_limit[i]);
					param.ft[i] = std::max(param.ft[i], -ft_limit[i]);
				}

				//����ѧ�غ�
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					//����ѧ����
					//constexpr double f_static[6] = { 9.349947583,11.64080253,4.770140543,3.631416685,2.58310847,1.783739862 };
					//constexpr double f_vel[6] = { 7.80825641,13.26518528,7.856443575,3.354615249,1.419632126,0.319206404 };
					//constexpr double f_acc[6] = { 0,3.555679326,0.344454603,0.148247716,0.048552673,0.033815455 };
					//constexpr double f2c_index[6] = { 9.07327526291993, 9.07327526291993, 17.5690184835913, 39.0310903520972, 66.3992503259041, 107.566785527965 };
					//constexpr double f_static_index[6] = {0.5, 0.5, 0.5, 0.85, 0.95, 0.8};

					//��Ħ����+��Ħ����=ft_friction

					real_vel[i] = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
					ft_friction1[i] = 0.8*(f_static[i] * real_vel[i] / max_static_vel[i]);

					//double ft_friction2_max = std::max(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? f_static[i] - ft_friction1[i] : f_static[i] + ft_friction1[i]);
					//double ft_friction2_min = std::min(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? -f_static[i] + ft_friction1[i] : -f_static[i] - ft_friction1[i]);
					//ft_friction2[i] = std::max(ft_friction2_min, std::min(ft_friction2_max, ft_friction2_index[i] * param.ft[i]));
					//ft_friction[i] = ft_friction1[i] + ft_friction2[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

					ft_friction[i] = ft_friction1[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

					//auto real_vel = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
					//ft_friction = (f_vel[i] * controller->motionAtAbs(i).actualVel() + f_static_index[i] * f_static[i] * real_vel / max_static_vel[i])*f2c_index[i];

					ft_friction[i] = std::max(-500.0, ft_friction[i]);
					ft_friction[i] = std::min(500.0, ft_friction[i]);

					//����ѧ�غ�=ft_dynamic
					ft_dynamic[i] = target.model->motionPool()[i].mfDyn();

					ft_offset[i] = (ft_friction[i] + ft_dynamic[i] + param.ft[i])*f2c_index[i];
					controller->motionAtAbs(i).setTargetCur(ft_offset[i]);
				}
			}

			//print//
			auto &cout = controller->mout();
			if (target.count % 1000 == 0)
			{
				cout << "pt:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << param.pt[i] << "  ";
				}
				cout << std::endl;

				cout << "pa:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << param.pa[i] << "  ";
				}
				cout << std::endl;

				cout << "vt:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << param.vt[i] << "  ";
				}
				cout << std::endl;

				cout << "va:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << param.va[i] << "  ";
				}
				cout << std::endl;

				cout << "vproportion:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << vproportion[i] << "  ";
				}
				cout << std::endl;

				cout << "vinteg:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << vinteg[i] << "  ";
				}
				cout << std::endl;

				cout << "ft:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << param.ft[i] << "  ";
				}
				cout << std::endl;
				cout << "friction1:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << ft_friction1[i] << "  ";
				}
				cout << std::endl;
				cout << "friction:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << ft_friction[i] << "  ";
				}
				cout << std::endl;
				cout << "ft_dynamic:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << ft_dynamic[i] << "  ";
				}
				cout << std::endl;
				cout << "ft_offset:";
				for (Size i = 0; i < 6; i++)
				{
					cout << std::setw(10) << ft_offset[i] << "  ";
				}
				cout << std::endl;
				cout << "------------------------------------------------" << std::endl;
			}

			// log //
			auto &lout = controller->lout();
			for (Size i = 0; i < param.ft.size(); i++)
			{
				lout << param.pt[i] << ",";
				lout << controller->motionAtAbs(i).actualPos() << ",";
				lout << param.vt[i] << ",";
				lout << controller->motionAtAbs(i).actualVel() << ",";
				lout << ft_offset[i] << ",";
				lout << controller->motionAtAbs(i).actualCur() << ",";
				lout << vproportion[i] << ",";
				lout << vinteg[i] << ",";
				lout << param.ft[i] << ",";
				lout << ft_friction1[i] << ",";
				lout << ft_friction[i] << ",";
				lout << ft_dynamic[i] << ",";
			}
			lout << std::endl;

			return (!is_running&&ds_is_all_finished&&md_is_all_finished) ? 0 : 1;
		}
	auto MoveJCrash::collectNrt(PlanTarget &target)->void {}
	MoveJCrash::MoveJCrash(const std::string &name) :Plan(name)
		{
			command().loadXmlStr(
				"<moveJCrash>"
				"	<group type=\"GroupParam\" default_child_type=\"Param\">"
				"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"pqt\">"
				"			<pqt default=\"{0.42,0.0,0.55,0,0,0,1}\" abbreviation=\"p\"/>"
				"			<xyz default=\"{0.01,0.0,0.0}\"/>"
				"		</unique>"
				"		<vel default=\"0.05\"/>"
				"		<acc default=\"0.1\"/>"
				"		<dec default=\"0.1\"/>"
				"		<kp_p default=\"{10,12,70,4,6,3}\"/>"
				"		<kp_v default=\"{200,360,120,100,60,20}\"/>"
				"		<ki_v default=\"{2,18,20,0.6,0.5,0.4}\"/>"
				"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_all\">"
				"			<check_all/>"
				"			<check_none/>"
				"			<group type=\"GroupParam\" default_child_type=\"Param\">"
				"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos\">"
				"					<check_pos/>"
				"					<not_check_pos/>"
				"					<group type=\"GroupParam\" default_child_type=\"Param\">"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_max\">"
				"							<check_pos_max/>"
				"							<not_check_pos_max/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_min\">"
				"							<check_pos_min/>"
				"							<not_check_pos_min/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous\">"
				"							<check_pos_continuous/>"
				"							<not_check_pos_continuous/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_at_start\">"
				"							<check_pos_continuous_at_start/>"
				"							<not_check_pos_continuous_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order\">"
				"							<check_pos_continuous_second_order/>"
				"							<not_check_pos_continuous_second_order/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order_at_start\">"
				"							<check_pos_continuous_second_order_at_start/>"
				"							<not_check_pos_continuous_second_order_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_following_error\">"
				"							<check_pos_following_error/>"
				"							<not_check_pos_following_error />"
				"						</unique>"
				"					</group>"
				"				</unique>"
				"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel\">"
				"					<check_vel/>"
				"					<not_check_vel/>"
				"					<group type=\"GroupParam\" default_child_type=\"Param\">"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_max\">"
				"							<check_vel_max/>"
				"							<not_check_vel_max/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_min\">"
				"							<check_vel_min/>"
				"							<not_check_vel_min/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous\">"
				"							<check_vel_continuous/>"
				"							<not_check_vel_continuous/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous_at_start\">"
				"							<check_vel_continuous_at_start/>"
				"							<not_check_vel_continuous_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_following_error\">"
				"							<check_vel_following_error/>"
				"							<not_check_vel_following_error />"
				"						</unique>"
				"					</group>"
				"				</unique>"
				"			</group>"
				"		</unique>"
				"	</group>"
				"</moveJCrash>");
		}


	// ���ظ��桪��ĩ��pq��MoveSetPQ������Ȼ��ĩ��pq���⵽��ռ䣬ͨ����ռ�PID���Ƶ�����������ٶ�ǰ������������ //
	struct MoveJFParam
	{
		std::vector<double> kp_p;
		std::vector<double> kp_v;
		std::vector<double> ki_v;

		std::vector<double> pqt;
		std::vector<double> pt;
		std::vector<double> pa;
		std::vector<double> vt;
		std::vector<double> va;
		std::vector<double> vfwd;

		std::vector<double> ft;
	};
	static std::atomic_bool enable_moveJF = true;
    static std::atomic<std::array<double, 7> > setpqJF;
	auto MoveJF::prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
	{
		auto c = dynamic_cast<aris::control::Controller*>(target.master);
		MoveJFParam param;
		enable_moveJF = true;
		param.kp_p.resize(6, 0.0);
		param.kp_v.resize(6, 0.0);
		param.ki_v.resize(6, 0.0);

		param.pqt.resize(7, 0.0);
		param.pt.resize(6, 0.0);
		param.pa.resize(6, 0.0);
		param.vt.resize(6, 0.0);
		param.va.resize(6, 0.0);
		param.vfwd.resize(6, 0.0);
		param.ft.resize(6, 0.0);

		for (auto &p : params)
		{
			if (p.first == "pqt")
			{
				auto pqarray = target.model->calculator().calculateExpression(p.second);
				param.pqt.assign(pqarray.begin(), pqarray.end());
				std::array<double, 7> temp;
				std::copy(pqarray.begin(), pqarray.end(), temp.begin());
				setpqJF.store(temp);
			}
			else if (p.first == "kp_p")
			{
				auto v = target.model->calculator().calculateExpression(p.second);
				if (v.size() == 1)
				{
					std::fill(param.kp_p.begin(), param.kp_p.end(), v.toDouble());
				}
				else if (v.size() == param.kp_p.size())
				{
					param.kp_p.assign(v.begin(), v.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}

				for (Size i = 0; i < param.kp_p.size(); ++i)
				{
					if (param.kp_p[i] > 10000.0)
					{
						param.kp_p[i] = 10000.0;
					}
					if (param.kp_p[i] < 0.01)
					{
						param.kp_p[i] = 0.01;
					}
				}
			}
			else if (p.first == "kp_v")
			{
				auto a = target.model->calculator().calculateExpression(p.second);
				if (a.size() == 1)
				{
					std::fill(param.kp_v.begin(), param.kp_v.end(), a.toDouble());
				}
				else if (a.size() == param.kp_v.size())
				{
					param.kp_v.assign(a.begin(), a.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}

				for (Size i = 0; i < param.kp_v.size(); ++i)
				{
					if (param.kp_v[i] > 10000.0)
					{
						param.kp_v[i] = 10000.0;
					}
					if (param.kp_v[i] < 0.01)
					{
						param.kp_v[i] = 0.01;
					}
				}
			}
			else if (p.first == "ki_v")
			{
				auto d = target.model->calculator().calculateExpression(p.second);
				if (d.size() == 1)
				{
					std::fill(param.ki_v.begin(), param.ki_v.end(), d.toDouble());
				}
				else if (d.size() == param.ki_v.size())
				{
					param.ki_v.assign(d.begin(), d.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}

				for (Size i = 0; i < param.ki_v.size(); ++i)
				{
					if (param.ki_v[i] > 10000.0)
					{
						param.ki_v[i] = 10000.0;
					}
					if (param.ki_v[i] < 0.001)
					{
						param.ki_v[i] = 0.001;
					}
				}
			}

		}
		target.param = param;

		target.option |=
			Plan::USE_VEL_OFFSET |
			//#ifdef WIN32
			Plan::NOT_CHECK_POS_MIN |
			Plan::NOT_CHECK_POS_MAX |
			Plan::NOT_CHECK_POS_CONTINUOUS |
			Plan::NOT_CHECK_POS_CONTINUOUS_AT_START |
			Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER |
			Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER_AT_START |
			Plan::NOT_CHECK_POS_FOLLOWING_ERROR |
			//#endif
			Plan::NOT_CHECK_VEL_MIN |
			Plan::NOT_CHECK_VEL_MAX |
			Plan::NOT_CHECK_VEL_CONTINUOUS |
			Plan::NOT_CHECK_VEL_CONTINUOUS_AT_START |
			Plan::NOT_CHECK_VEL_FOLLOWING_ERROR;

	}
	auto MoveJF::executeRT(PlanTarget &target)->int
	{
		auto &param = std::any_cast<MoveJFParam&>(target.param);
		auto controller = dynamic_cast<aris::control::Controller *>(target.master);
		static bool is_running{ true };
		static double vinteg[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		static double vproportion[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		bool ds_is_all_finished{ true };
		bool md_is_all_finished{ true };

		//��һ�����ڣ���Ŀ�����Ŀ���ģʽ�л�����������ģʽ		
		if (target.count == 1)
		{
			is_running = true;

			for (Size i = 0; i < param.ft.size(); ++i)
			{
				controller->motionPool().at(i).setModeOfOperation(10);	//�л�����������
			}
		}

		//���һ�����ڽ�Ŀ����ȥʹ��
		if (!enable_moveJF)
		{
			is_running = false;
		}
		if (!is_running)
		{
			for (Size i = 0; i < param.ft.size(); ++i)
			{
				auto ret = controller->motionPool().at(i).disable();
				if (ret)
				{
					ds_is_all_finished = false;
				}
			}
		}

		//��Ŀ�����ɵ���ģʽ�л���λ��ģʽ
		if (!is_running&&ds_is_all_finished)
		{
			for (Size i = 0; i < param.ft.size(); ++i)
			{
				controller->motionPool().at(i).setModeOfOperation(8);
				auto ret = controller->motionPool().at(i).mode(8);
				if (ret)
				{
					md_is_all_finished = false;
				}
			}
		}

		//ͨ��moveSPQ����ʵʱĿ��PQλ��
		std::array<double, 7> temp;
		temp = setpqJF.load();
		std::copy(temp.begin(), temp.end(), param.pqt.begin());
		
		//��Ŀ��λ��pq���˶�ѧ���⣬��ȡ���ʵ��λ�á�ʵ���ٶ�
		target.model->generalMotionPool().at(0).setMpq(param.pqt.data());
		if (!target.model->solverPool().at(0).kinPos())return -1;
		for (Size i = 0; i < param.pt.size(); ++i)
		{
			param.pt[i] = target.model->motionPool().at(i).mp();		//motionPool()ָģ����������at(0)��ʾ��1��������
			param.pa[i] = controller->motionPool().at(i).actualPos();
			param.va[i] = controller->motionPool().at(i).actualVel();
		}
		
		//ģ������
		for (int i = 0; i < param.ft.size(); ++i)
		{
			target.model->motionPool()[i].setMp(controller->motionPool()[i].actualPos());
			target.model->motionPool().at(i).setMv(controller->motionAtAbs(i).actualVel());
			target.model->motionPool().at(i).setMa(0.0);
		}
		target.model->solverPool()[1].kinPos();
		target.model->solverPool()[1].kinVel();
		target.model->solverPool()[2].dynAccAndFce();

		double ft_friction[6];
		double ft_offset[6];
		double real_vel[6];
		double ft_friction1[6], ft_friction2[6], ft_dynamic[6];
		static double ft_friction2_index[6] = { 5.0, 5.0, 5.0, 5.0, 5.0, 3.0 };
		if (is_running)
		{
			//λ�û�PID+�ٶ�����
			for (Size i = 0; i < param.kp_p.size(); ++i)
			{
				param.vt[i] = param.kp_p[i] * (param.pt[i] - param.pa[i]);
				param.vt[i] = std::max(std::min(param.vt[i], vt_limit[i]), -vt_limit[i]);
				param.vt[i] = param.vt[i] + param.vfwd[i];
			}

			//�ٶȻ�PID+�������ص�����
			for (Size i = 0; i < param.ft.size(); ++i)
			{
				vproportion[i] = param.kp_v[i] * (param.vt[i] - param.va[i]);
				vinteg[i] = vinteg[i] + param.ki_v[i] * (param.vt[i] - param.va[i]);
				vinteg[i] = std::min(vinteg[i], fi_limit[i]);
				vinteg[i] = std::max(vinteg[i], -fi_limit[i]);

				param.ft[i] = vproportion[i] + vinteg[i];
				param.ft[i] = std::min(param.ft[i], ft_limit[i]);
				param.ft[i] = std::max(param.ft[i], -ft_limit[i]);
			}

			//����ѧ�غ�
			for (Size i = 0; i < param.ft.size(); ++i)
			{
				//����ѧ����
				//constexpr double f_static[6] = { 9.349947583,11.64080253,4.770140543,3.631416685,2.58310847,1.783739862 };
				//constexpr double f_vel[6] = { 7.80825641,13.26518528,7.856443575,3.354615249,1.419632126,0.319206404 };
				//constexpr double f_acc[6] = { 0,3.555679326,0.344454603,0.148247716,0.048552673,0.033815455 };
				//constexpr double f2c_index[6] = { 9.07327526291993, 9.07327526291993, 17.5690184835913, 39.0310903520972, 66.3992503259041, 107.566785527965 };
				//constexpr double f_static_index[6] = {0.5, 0.5, 0.5, 0.85, 0.95, 0.8};

				//��Ħ����+��Ħ����=ft_friction

				real_vel[i] = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
				ft_friction1[i] = 0.8*(f_static[i] * real_vel[i] / max_static_vel[i]);

				//double ft_friction2_max = std::max(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? f_static[i] - ft_friction1[i] : f_static[i] + ft_friction1[i]);
				//double ft_friction2_min = std::min(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? -f_static[i] + ft_friction1[i] : -f_static[i] - ft_friction1[i]);
				//ft_friction2[i] = std::max(ft_friction2_min, std::min(ft_friction2_max, ft_friction2_index[i] * param.ft[i]));
				//ft_friction[i] = ft_friction1[i] + ft_friction2[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

				ft_friction[i] = ft_friction1[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

				//auto real_vel = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
				//ft_friction = (f_vel[i] * controller->motionAtAbs(i).actualVel() + f_static_index[i] * f_static[i] * real_vel / max_static_vel[i])*f2c_index[i];

				ft_friction[i] = std::max(-500.0, ft_friction[i]);
				ft_friction[i] = std::min(500.0, ft_friction[i]);

				//����ѧ�غ�=ft_dynamic
				ft_dynamic[i] = target.model->motionPool()[i].mfDyn();

				ft_offset[i] = (ft_friction[i] + ft_dynamic[i] + param.ft[i])*f2c_index[i];
				controller->motionAtAbs(i).setTargetCur(ft_offset[i]);
			}
		}

		//print//
		auto &cout = controller->mout();
		if (target.count % 1000 == 0)
		{
			cout << "pt:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.pt[i] << "  ";
			}
			cout << std::endl;

			cout << "pa:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.pa[i] << "  ";
			}
			cout << std::endl;

			cout << "vt:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.vt[i] << "  ";
			}
			cout << std::endl;

			cout << "va:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.va[i] << "  ";
			}
			cout << std::endl;

			cout << "vproportion:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << vproportion[i] << "  ";
			}
			cout << std::endl;

			cout << "vinteg:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << vinteg[i] << "  ";
			}
			cout << std::endl;

			cout << "ft:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.ft[i] << "  ";
			}
			cout << std::endl;
			cout << "friction1:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << ft_friction1[i] << "  ";
			}
			cout << std::endl;
			cout << "friction:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << ft_friction[i] << "  ";
			}
			cout << std::endl;
			cout << "ft_dynamic:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << ft_dynamic[i] << "  ";
			}
			cout << std::endl;
			cout << "ft_offset:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << ft_offset[i] << "  ";
			}
			cout << std::endl;
			cout << "------------------------------------------------" << std::endl;
		}

		// log //
		auto &lout = controller->lout();
		for (Size i = 0; i < param.ft.size(); i++)
		{
			lout << param.pt[i] << ",";
			lout << controller->motionAtAbs(i).actualPos() << ",";
			lout << param.vt[i] << ",";
			lout << controller->motionAtAbs(i).actualVel() << ",";
			lout << ft_offset[i] << ",";
			lout << controller->motionAtAbs(i).actualCur() << ",";
			lout << vproportion[i] << ",";
			lout << vinteg[i] << ",";
			lout << param.ft[i] << ",";
			lout << ft_friction1[i] << ",";
			lout << ft_friction[i] << ",";
			lout << ft_dynamic[i] << ",";
		}
		lout << std::endl;

		return (!is_running&&ds_is_all_finished&&md_is_all_finished) ? 0 : 1;
	}
	auto MoveJF::collectNrt(PlanTarget &target)->void {}
	MoveJF::MoveJF(const std::string &name) :Plan(name)
	{
		command().loadXmlStr(
			"<moveJF>"
			"	<group type=\"GroupParam\" default_child_type=\"Param\">"
			"		<pqt default=\"{0.42,0.0,0.55,0,0,0,1}\" abbreviation=\"p\"/>"
            "		<kp_p default=\"{10,12,20,3,4,3}\"/>"
            "		<kp_v default=\"{200,360,120,80,40,20}\"/>"
            "		<ki_v default=\"{2,18,20,0.4,0.3,0.4}\"/>"
			"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_all\">"
			"			<check_all/>"
			"			<check_none/>"
			"			<group type=\"GroupParam\" default_child_type=\"Param\">"
			"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos\">"
			"					<check_pos/>"
			"					<not_check_pos/>"
			"					<group type=\"GroupParam\" default_child_type=\"Param\">"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_max\">"
			"							<check_pos_max/>"
			"							<not_check_pos_max/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_min\">"
			"							<check_pos_min/>"
			"							<not_check_pos_min/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous\">"
			"							<check_pos_continuous/>"
			"							<not_check_pos_continuous/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_at_start\">"
			"							<check_pos_continuous_at_start/>"
			"							<not_check_pos_continuous_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order\">"
			"							<check_pos_continuous_second_order/>"
			"							<not_check_pos_continuous_second_order/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order_at_start\">"
			"							<check_pos_continuous_second_order_at_start/>"
			"							<not_check_pos_continuous_second_order_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_following_error\">"
			"							<check_pos_following_error/>"
			"							<not_check_pos_following_error />"
			"						</unique>"
			"					</group>"
			"				</unique>"
			"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel\">"
			"					<check_vel/>"
			"					<not_check_vel/>"
			"					<group type=\"GroupParam\" default_child_type=\"Param\">"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_max\">"
			"							<check_vel_max/>"
			"							<not_check_vel_max/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_min\">"
			"							<check_vel_min/>"
			"							<not_check_vel_min/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous\">"
			"							<check_vel_continuous/>"
			"							<not_check_vel_continuous/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous_at_start\">"
			"							<check_vel_continuous_at_start/>"
			"							<not_check_vel_continuous_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_following_error\">"
			"							<check_vel_following_error/>"
			"							<not_check_vel_following_error />"
			"						</unique>"
			"					</group>"
			"				</unique>"
			"			</group>"
			"		</unique>"
			"	</group>"
			"</moveJF>");
	}


	// ���ظ��桪��ĩ��pq��MoveSetPQ��������������ᱣ�ֲ��������ٶ�ǰ������������ //
	struct MoveJFBParam
	{
		std::vector<double> kp_p;
		std::vector<double> kp_v;
		std::vector<double> ki_v;

		std::vector<double> pqt;
		std::vector<double> pqa;
		std::vector<double> pt;
		std::vector<double> pa;
		std::vector<double> vt;
		std::vector<double> va;
		std::vector<double> vfwd;

		std::vector<double> ft;
	};
	static std::atomic_bool enable_moveJFB = true;
	static std::atomic<std::array<double, 7> > setpqJFB;
	auto MoveJFB::prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
	{
		auto c = dynamic_cast<aris::control::Controller*>(target.master);
		MoveJFBParam param;
		enable_moveJFB = true;
		param.kp_p.resize(6, 0.0);
		param.kp_v.resize(6, 0.0);
		param.ki_v.resize(6, 0.0);

		param.pqt.resize(7, 0.0);
		param.pqa.resize(7, 0.0);
		param.pt.resize(6, 0.0);
		param.pa.resize(6, 0.0);
		param.vt.resize(6, 0.0);
		param.va.resize(6, 0.0);
		param.vfwd.resize(6, 0.0);
		param.ft.resize(6, 0.0);

		for (auto &p : params)
		{
			if (p.first == "pqt")
			{
				auto pqarray = target.model->calculator().calculateExpression(p.second);
				param.pqt.assign(pqarray.begin(), pqarray.end());
				std::array<double, 7> temp;
				std::copy(pqarray.begin(), pqarray.end(), temp.begin());
				setpqJFB.store(temp);
			}
			else if (p.first == "kp_p")
			{
				auto v = target.model->calculator().calculateExpression(p.second);
				if (v.size() == 1)
				{
					std::fill(param.kp_p.begin(), param.kp_p.end(), v.toDouble());
				}
				else if (v.size() == param.kp_p.size())
				{
					param.kp_p.assign(v.begin(), v.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}

				for (Size i = 0; i < param.kp_p.size(); ++i)
				{
					if (param.kp_p[i] > 10000.0)
					{
						param.kp_p[i] = 10000.0;
					}
					if (param.kp_p[i] < 0.01)
					{
						param.kp_p[i] = 0.01;
					}
				}
			}
			else if (p.first == "kp_v")
			{
				auto a = target.model->calculator().calculateExpression(p.second);
				if (a.size() == 1)
				{
					std::fill(param.kp_v.begin(), param.kp_v.end(), a.toDouble());
				}
				else if (a.size() == param.kp_v.size())
				{
					param.kp_v.assign(a.begin(), a.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}

				for (Size i = 0; i < param.kp_v.size(); ++i)
				{
					if (param.kp_v[i] > 10000.0)
					{
						param.kp_v[i] = 10000.0;
					}
					if (param.kp_v[i] < 0.01)
					{
						param.kp_v[i] = 0.01;
					}
				}
			}
			else if (p.first == "ki_v")
			{
				auto d = target.model->calculator().calculateExpression(p.second);
				if (d.size() == 1)
				{
					std::fill(param.ki_v.begin(), param.ki_v.end(), d.toDouble());
				}
				else if (d.size() == param.ki_v.size())
				{
					param.ki_v.assign(d.begin(), d.end());
				}
				else
				{
					throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
				}

				for (Size i = 0; i < param.ki_v.size(); ++i)
				{
					if (param.ki_v[i] > 10000.0)
					{
						param.ki_v[i] = 10000.0;
					}
					if (param.ki_v[i] < 0.001)
					{
						param.ki_v[i] = 0.001;
					}
				}
			}

		}
		target.param = param;

		target.option |=
			Plan::USE_VEL_OFFSET |
			//#ifdef WIN32
			Plan::NOT_CHECK_POS_MIN |
			Plan::NOT_CHECK_POS_MAX |
			Plan::NOT_CHECK_POS_CONTINUOUS |
			Plan::NOT_CHECK_POS_CONTINUOUS_AT_START |
			Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER |
			Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER_AT_START |
			Plan::NOT_CHECK_POS_FOLLOWING_ERROR |
			//#endif
			Plan::NOT_CHECK_VEL_MIN |
			Plan::NOT_CHECK_VEL_MAX |
			Plan::NOT_CHECK_VEL_CONTINUOUS |
			Plan::NOT_CHECK_VEL_CONTINUOUS_AT_START |
			Plan::NOT_CHECK_VEL_FOLLOWING_ERROR;

	}
	auto MoveJFB::executeRT(PlanTarget &target)->int
	{
		auto &param = std::any_cast<MoveJFBParam&>(target.param);
		auto controller = dynamic_cast<aris::control::Controller *>(target.master);
		static bool is_running{ true };
		static double vinteg[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		static double vproportion[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		bool ds_is_all_finished{ true };
		bool md_is_all_finished{ true };

		//��һ�����ڣ���Ŀ�����Ŀ���ģʽ�л�����������ģʽ		
		if (target.count == 1)
		{
			is_running = true;

			for (Size i = 0; i < param.ft.size(); ++i)
			{
				controller->motionPool().at(i).setModeOfOperation(10);	//�л�����������
			}
		}

		//���һ�����ڽ�Ŀ����ȥʹ��
		if (!enable_moveJFB)
		{
			is_running = false;
		}
		if (!is_running)
		{
			for (Size i = 0; i < param.ft.size(); ++i)
			{
				auto ret = controller->motionPool().at(i).disable();
				if (ret)
				{
					ds_is_all_finished = false;
				}
			}
		}

		//��Ŀ�����ɵ���ģʽ�л���λ��ģʽ
		if (!is_running&&ds_is_all_finished)
		{
			for (Size i = 0; i < param.ft.size(); ++i)
			{
				controller->motionPool().at(i).setModeOfOperation(8);
				auto ret = controller->motionPool().at(i).mode(8);
				if (ret)
				{
					md_is_all_finished = false;
				}
			}
		}

		//ͨ��moveSPQ����ʵʱĿ��PQλ��
		std::array<double, 7> temp;
		temp = setpqJFB.load();
		std::copy(temp.begin(), temp.end(), param.pqt.begin());

		//��Ŀ��λ��pq���˶�ѧ���⣬��ȡ���ʵ��λ�á�ʵ���ٶ�
		target.model->generalMotionPool().at(0).setMpq(param.pqt.data());
		if (!target.model->solverPool().at(0).kinPos())return -1;
		for (Size i = 0; i < param.pt.size(); ++i)
		{
			param.pt[i] = target.model->motionPool().at(i).mp();		//motionPool()ָģ����������at(0)��ʾ��1��������
			param.pa[i] = controller->motionPool().at(i).actualPos();
			param.va[i] = controller->motionPool().at(i).actualVel();
		}

		//ģ���˶�ѧ���⡢����ѧ����
		for (int i = 0; i < param.ft.size(); ++i)
		{
			target.model->motionPool()[i].setMp(controller->motionPool()[i].actualPos());
			target.model->motionPool().at(i).setMv(controller->motionAtAbs(i).actualVel());
			target.model->motionPool().at(i).setMa(0.0);
		}
		target.model->solverPool()[1].kinPos();
		target.model->solverPool()[1].kinVel();
		target.model->solverPool()[2].dynAccAndFce();

		//�ǶȲ��䣬λ�ñ仯
		target.model->generalMotionPool().at(0).getMpq(param.pqa.data());
		std::array<double, 4> q = {0.0,0.0,0.0,1.0};
		std::copy(q.begin(), q.end(), param.pqa.begin()+3);
		target.model->generalMotionPool().at(0).setMpq(param.pqa.data());
		if (!target.model->solverPool().at(1).kinPos())return -1;
		for (Size i = 3; i < param.pt.size(); ++i)
		{
			param.pt[i] = target.model->motionPool().at(i).mp();		//motionPool()ָģ����������at(0)��ʾ��1��������
			param.pa[i] = controller->motionPool().at(i).actualPos();
			param.va[i] = controller->motionPool().at(i).actualVel();
		}

		double ft_friction[6];
		double ft_offset[6];
		double real_vel[6];
		double ft_friction1[6], ft_friction2[6], ft_dynamic[6];
		static double ft_friction2_index[6] = { 5.0, 5.0, 5.0, 5.0, 5.0, 3.0 };
		if (is_running)
		{
			//λ�û�PID+�ٶ�����
			for (Size i = 0; i < param.kp_p.size(); ++i)
			{
				param.vt[i] = param.kp_p[i] * (param.pt[i] - param.pa[i]);
				param.vt[i] = std::max(std::min(param.vt[i], vt_limit_JFB[i]), -vt_limit_JFB[i]);
				param.vt[i] = param.vt[i] + param.vfwd[i];
			}

			//�ٶȻ�PID+�������ص�����
			for (Size i = 0; i < param.ft.size(); ++i)
			{
				vproportion[i] = param.kp_v[i] * (param.vt[i] - param.va[i]);
				vinteg[i] = vinteg[i] + param.ki_v[i] * (param.vt[i] - param.va[i]);
				vinteg[i] = std::min(vinteg[i], fi_limit_JFB[i]);
				vinteg[i] = std::max(vinteg[i], -fi_limit_JFB[i]);

				param.ft[i] = vproportion[i] + vinteg[i];
				param.ft[i] = std::min(param.ft[i], ft_limit_JFB[i]);
				param.ft[i] = std::max(param.ft[i], -ft_limit_JFB[i]);
			}

			//����ѧ�غ�
			for (Size i = 0; i < param.ft.size(); ++i)
			{
				//����ѧ����
				//constexpr double f_static[6] = { 9.349947583,11.64080253,4.770140543,3.631416685,2.58310847,1.783739862 };
				//constexpr double f_vel[6] = { 7.80825641,13.26518528,7.856443575,3.354615249,1.419632126,0.319206404 };
				//constexpr double f_acc[6] = { 0,3.555679326,0.344454603,0.148247716,0.048552673,0.033815455 };
				//constexpr double f2c_index[6] = { 9.07327526291993, 9.07327526291993, 17.5690184835913, 39.0310903520972, 66.3992503259041, 107.566785527965 };
				//constexpr double f_static_index[6] = {0.5, 0.5, 0.5, 0.85, 0.95, 0.8};

				//��Ħ����+��Ħ����=ft_friction

				real_vel[i] = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
				ft_friction1[i] = 0.8*(f_static[i] * real_vel[i] / max_static_vel[i]);

				//double ft_friction2_max = std::max(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? f_static[i] - ft_friction1[i] : f_static[i] + ft_friction1[i]);
				//double ft_friction2_min = std::min(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? -f_static[i] + ft_friction1[i] : -f_static[i] - ft_friction1[i]);
				//ft_friction2[i] = std::max(ft_friction2_min, std::min(ft_friction2_max, ft_friction2_index[i] * param.ft[i]));
				//ft_friction[i] = ft_friction1[i] + ft_friction2[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

				ft_friction[i] = ft_friction1[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

				//auto real_vel = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
				//ft_friction = (f_vel[i] * controller->motionAtAbs(i).actualVel() + f_static_index[i] * f_static[i] * real_vel / max_static_vel[i])*f2c_index[i];

				ft_friction[i] = std::max(-500.0, ft_friction[i]);
				ft_friction[i] = std::min(500.0, ft_friction[i]);

				//����ѧ�غ�=ft_dynamic
				ft_dynamic[i] = target.model->motionPool()[i].mfDyn();

				ft_offset[i] = (ft_friction[i] + ft_dynamic[i] + param.ft[i])*f2c_index[i];
				controller->motionAtAbs(i).setTargetCur(ft_offset[i]);
			}
		}

		//print//
		auto &cout = controller->mout();
		if (target.count % 1000 == 0)
		{
			cout << "pt:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.pt[i] << "  ";
			}
			cout << std::endl;

			cout << "pa:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.pa[i] << "  ";
			}
			cout << std::endl;

			cout << "vt:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.vt[i] << "  ";
			}
			cout << std::endl;

			cout << "va:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.va[i] << "  ";
			}
			cout << std::endl;

			cout << "vproportion:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << vproportion[i] << "  ";
			}
			cout << std::endl;

			cout << "vinteg:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << vinteg[i] << "  ";
			}
			cout << std::endl;

			cout << "ft:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << param.ft[i] << "  ";
			}
			cout << std::endl;
			cout << "friction1:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << ft_friction1[i] << "  ";
			}
			cout << std::endl;
			cout << "friction:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << ft_friction[i] << "  ";
			}
			cout << std::endl;
			cout << "ft_dynamic:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << ft_dynamic[i] << "  ";
			}
			cout << std::endl;
			cout << "ft_offset:";
			for (Size i = 0; i < 6; i++)
			{
				cout << std::setw(10) << ft_offset[i] << "  ";
			}
			cout << std::endl;
			cout << "------------------------------------------------" << std::endl;
		}

		//log//
		auto &lout = controller->lout();
		for (Size i = 0; i < param.ft.size(); i++)
		{
			lout << param.pt[i] << ",";
			lout << controller->motionAtAbs(i).actualPos() << ",";
			lout << param.vt[i] << ",";
			lout << controller->motionAtAbs(i).actualVel() << ",";
			lout << ft_offset[i] << ",";
			lout << controller->motionAtAbs(i).actualCur() << ",";
			lout << vproportion[i] << ",";
			lout << vinteg[i] << ",";
			lout << param.ft[i] << ",";
			lout << ft_friction1[i] << ",";
			lout << ft_friction[i] << ",";
			lout << ft_dynamic[i] << ",";
		}
		lout << std::endl;

		return (!is_running&&ds_is_all_finished&&md_is_all_finished) ? 0 : 1;
	}
	auto MoveJFB::collectNrt(PlanTarget &target)->void {}
	MoveJFB::MoveJFB(const std::string &name) :Plan(name)
	{
		command().loadXmlStr(
			"<moveJFB>"
			"	<group type=\"GroupParam\" default_child_type=\"Param\">"
			"		<pqt default=\"{0.42,0.0,0.55,0,0,0,1}\" abbreviation=\"p\"/>"
			"		<kp_p default=\"{10,12,20,3,4,3}\"/>"
			"		<kp_v default=\"{200,360,120,80,40,20}\"/>"
			"		<ki_v default=\"{2,18,20,0.4,0.3,0.4}\"/>"
			"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_all\">"
			"			<check_all/>"
			"			<check_none/>"
			"			<group type=\"GroupParam\" default_child_type=\"Param\">"
			"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos\">"
			"					<check_pos/>"
			"					<not_check_pos/>"
			"					<group type=\"GroupParam\" default_child_type=\"Param\">"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_max\">"
			"							<check_pos_max/>"
			"							<not_check_pos_max/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_min\">"
			"							<check_pos_min/>"
			"							<not_check_pos_min/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous\">"
			"							<check_pos_continuous/>"
			"							<not_check_pos_continuous/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_at_start\">"
			"							<check_pos_continuous_at_start/>"
			"							<not_check_pos_continuous_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order\">"
			"							<check_pos_continuous_second_order/>"
			"							<not_check_pos_continuous_second_order/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order_at_start\">"
			"							<check_pos_continuous_second_order_at_start/>"
			"							<not_check_pos_continuous_second_order_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_following_error\">"
			"							<check_pos_following_error/>"
			"							<not_check_pos_following_error />"
			"						</unique>"
			"					</group>"
			"				</unique>"
			"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel\">"
			"					<check_vel/>"
			"					<not_check_vel/>"
			"					<group type=\"GroupParam\" default_child_type=\"Param\">"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_max\">"
			"							<check_vel_max/>"
			"							<not_check_vel_max/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_min\">"
			"							<check_vel_min/>"
			"							<not_check_vel_min/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous\">"
			"							<check_vel_continuous/>"
			"							<not_check_vel_continuous/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous_at_start\">"
			"							<check_vel_continuous_at_start/>"
			"							<not_check_vel_continuous_at_start/>"
			"						</unique>"
			"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_following_error\">"
			"							<check_vel_following_error/>"
			"							<not_check_vel_following_error />"
			"						</unique>"
			"					</group>"
			"				</unique>"
			"			</group>"
			"		</unique>"
			"	</group>"
			"</moveJFB>");
	}


	// ����PID���������������뵥������ȫ����ռ�λ�ã�Ȼ��ֱ�Ը�����ռ����PID�������ٶ�ǰ������������ //
	struct MoveJPIDParam
	{
		std::vector<bool> joint_active_vec;
		std::vector<double> kp_p;
		std::vector<double> kp_v;
		std::vector<double> ki_v;
		std::vector<double> kd_v;

		std::vector<double> pt;
		std::vector<double> pa;
		std::vector<double> vt;
		std::vector<double> va;

		std::vector<double> ft;
	};
	static std::atomic_bool enable_moveJPID = true;
	auto MoveJPID::prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
		{
			auto c = dynamic_cast<aris::control::Controller*>(target.master);
			MoveJPIDParam param;
			enable_moveJPID = true;
			param.kp_p.resize(6, 0.0);
			param.kp_v.resize(6, 0.0);
			param.ki_v.resize(6, 0.0);
			param.kd_v.resize(6, 0.0);

			param.pa.resize(6, 0.0);
			param.vt.resize(6, 0.0);
			param.va.resize(6, 0.0);
			param.ft.resize(6, 0.0);

			for (auto &p : params)
			{
				if (p.first == "all")
				{
					param.joint_active_vec.resize(target.model->motionPool().size(), true);
				}
				else if (p.first == "none")
				{
					param.joint_active_vec.resize(target.model->motionPool().size(), false);
				}
				else if (p.first == "motion_id")
				{
					param.joint_active_vec.resize(target.model->motionPool().size(), false);
					param.joint_active_vec.at(std::stoi(p.second)) = true;
				}
				else if (p.first == "physical_id")
				{
					param.joint_active_vec.resize(c->motionPool().size(), false);
					param.joint_active_vec.at(c->motionAtPhy(std::stoi(p.second)).phyId()) = true;
				}
				else if (p.first == "slave_id")
				{
					param.joint_active_vec.resize(c->motionPool().size(), false);
					param.joint_active_vec.at(c->motionAtPhy(std::stoi(p.second)).slaId()) = true;
				}
				else if (p.first == "pos")
				{
					aris::core::Matrix mat = target.model->calculator().calculateExpression(p.second);
					if (mat.size() == 1)param.pt.resize(c->motionPool().size(), mat.toDouble());
					else
					{
						param.pt.resize(mat.size());
						std::copy(mat.begin(), mat.end(), param.pt.begin());
					}
				}
				else if (p.first == "kp_p")
				{
					auto v = target.model->calculator().calculateExpression(p.second);
					if (v.size() == 1)
					{
						std::fill(param.kp_p.begin(), param.kp_p.end(), v.toDouble());
					}
					else if (v.size() == param.kp_p.size())
					{
						param.kp_p.assign(v.begin(), v.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.kp_p.size(); ++i)
					{
						if (param.kp_p[i] > 10000.0)
						{
							param.kp_p[i] = 10000.0;
						}
						if (param.kp_p[i] < 0.01)
						{
							param.kp_p[i] = 0.01;
						}
					}
				}
				else if (p.first == "kp_v")
				{
					auto a = target.model->calculator().calculateExpression(p.second);
					if (a.size() == 1)
					{
						std::fill(param.kp_v.begin(), param.kp_v.end(), a.toDouble());
					}
					else if (a.size() == param.kp_v.size())
					{
						param.kp_v.assign(a.begin(), a.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.kp_v.size(); ++i)
					{
						if (param.kp_v[i] > 10000.0)
						{
							param.kp_v[i] = 10000.0;
						}
						if (param.kp_v[i] < 0.01)
						{
							param.kp_v[i] = 0.01;
						}
					}
				}
				else if (p.first == "ki_v")
				{
					auto d = target.model->calculator().calculateExpression(p.second);
					if (d.size() == 1)
					{
						std::fill(param.ki_v.begin(), param.ki_v.end(), d.toDouble());
					}
					else if (d.size() == param.ki_v.size())
					{
						param.ki_v.assign(d.begin(), d.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.ki_v.size(); ++i)
					{
						if (param.ki_v[i] > 10000.0)
						{
							param.ki_v[i] = 10000.0;
						}
						if (param.ki_v[i] < 0.001)
						{
							param.ki_v[i] = 0.001;
						}
					}
				}
				else if (p.first == "kd_v")
				{
					auto d = target.model->calculator().calculateExpression(p.second);
					if (d.size() == 1)
					{
						std::fill(param.kd_v.begin(), param.kd_v.end(), d.toDouble());
					}
					else if (d.size() == param.kd_v.size())
					{
						param.kd_v.assign(d.begin(), d.end());
					}
					else
					{
						throw std::runtime_error(__FILE__ + std::to_string(__LINE__) + " failed");
					}

					for (Size i = 0; i < param.kd_v.size(); ++i)
					{
						if (param.kd_v[i] > 1000.0)
						{
							param.kd_v[i] = 1000.0;
						}
						if (param.kd_v[i] < 0.0)
						{
							param.kd_v[i] = 0.0;
						}
					}
				}
			}
			target.param = param;

			target.option |=
				Plan::USE_VEL_OFFSET |
				//#ifdef WIN32
				Plan::NOT_CHECK_POS_MIN |
				Plan::NOT_CHECK_POS_MAX |
				Plan::NOT_CHECK_POS_CONTINUOUS |
				Plan::NOT_CHECK_POS_CONTINUOUS_AT_START |
				Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER |
				Plan::NOT_CHECK_POS_CONTINUOUS_SECOND_ORDER_AT_START |
				Plan::NOT_CHECK_POS_FOLLOWING_ERROR |
				//#endif
				Plan::NOT_CHECK_VEL_MIN |
				Plan::NOT_CHECK_VEL_MAX |
				Plan::NOT_CHECK_VEL_CONTINUOUS |
				Plan::NOT_CHECK_VEL_CONTINUOUS_AT_START |
				Plan::NOT_CHECK_VEL_FOLLOWING_ERROR;

		}
	auto MoveJPID::executeRT(PlanTarget &target)->int
		{
			auto &param = std::any_cast<MoveJPIDParam&>(target.param);
			auto controller = dynamic_cast<aris::control::Controller *>(target.master);
			static bool is_running{ true };
			static double vproportion[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			static double vinteg[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			static double vdiff[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			static double vt_va_last[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			static double vt_va_this[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			bool ds_is_all_finished{ true };
			bool md_is_all_finished{ true };

			//��һ�����ڣ���Ŀ�����Ŀ���ģʽ�л�����������ģʽ		
			if (target.count == 1)
			{
				is_running = true;
				for (Size i = 0; i < param.joint_active_vec.size(); ++i)
				{
					if (param.joint_active_vec[i])
					{
						controller->motionPool().at(i).setModeOfOperation(10);	//�л�����������
					}
				}
			}

			//���һ�����ڽ�Ŀ����ȥʹ��
			if (!enable_moveJPID)
			{
				is_running = false;
			}
			if (!is_running)
			{
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					auto ret = controller->motionPool().at(i).disable();
					if (ret)
					{
						ds_is_all_finished = false;
					}
				}
			}

			//��Ŀ�����ɵ���ģʽ�л���λ��ģʽ
			if (!is_running&&ds_is_all_finished)
			{
				for (Size i = 0; i < param.ft.size(); ++i)
				{
					controller->motionPool().at(i).setModeOfOperation(8);
					auto ret = controller->motionPool().at(i).mode(8);
					if (ret)
					{
						md_is_all_finished = false;
					}
				}
			}

			//��ȡ���ʵ��λ�á�ʵ���ٶ�
			for (Size i = 0; i < param.joint_active_vec.size(); ++i)
			{
				if (param.joint_active_vec[i])
				{
					param.pa[i] = controller->motionPool().at(i).actualPos();	//motionPool()ָģ����������at(0)��ʾ��1��������
					param.va[i] = controller->motionPool().at(i).actualVel();
				}
			}

			//ģ������
			for (int i = 0; i < param.ft.size(); ++i)
			{
				target.model->motionPool()[i].setMp(controller->motionPool()[i].actualPos());
				target.model->motionPool().at(i).setMv(controller->motionAtAbs(i).actualVel());
				target.model->motionPool().at(i).setMa(0.0);
			}
			target.model->solverPool()[1].kinPos();
			target.model->solverPool()[1].kinVel();
			target.model->solverPool()[2].dynAccAndFce();

			double ft_friction[6];
			double ft_offset[6];
			double real_vel[6];
			double ft_friction1[6], ft_friction2[6], ft_dynamic[6];
			static double ft_friction2_index[6] = { 5.0, 5.0, 5.0, 5.0, 5.0, 3.0 };
			if (is_running)
			{
				//λ�û�PID+�ٶ�����
				for (Size i = 0; i < param.joint_active_vec.size(); ++i)
				{
					if (param.joint_active_vec[i])
					{
						param.vt[i] = param.kp_p[i] * (param.pt[i] - param.pa[i]);
						param.vt[i] = std::max(std::min(param.vt[i], vt_limit[i]), -vt_limit[i]);
					}
				}

				//�ٶȻ�PID+�������ص�����
				for (Size i = 0; i < param.joint_active_vec.size(); ++i)
				{
					if (param.joint_active_vec[i])
					{
						vproportion[i] = param.kp_v[i] * (param.vt[i] - param.va[i]);

						vinteg[i] = vinteg[i] + param.ki_v[i] * (param.vt[i] - param.va[i]);
						vinteg[i] = std::min(vinteg[i], fi_limit[i]);
						vinteg[i] = std::max(vinteg[i], -fi_limit[i]);

						vt_va_this[i] = param.vt[i] - param.va[i];
						vdiff[i] = param.kd_v[i] * (vt_va_this[i] - vt_va_last[i]);
						vt_va_last[i] = param.vt[i] - param.va[i];

						param.ft[i] = vproportion[i] + vinteg[i] + vdiff[i];
					}
				}

				//����ѧ�غ�
				for (Size i = 0; i < param.joint_active_vec.size(); ++i)
				{
					if (param.joint_active_vec[i])
					{
						//����ѧ����
						//constexpr double f_static[6] = { 9.349947583,11.64080253,4.770140543,3.631416685,2.58310847,1.783739862 };
						//constexpr double f_vel[6] = { 7.80825641,13.26518528,7.856443575,3.354615249,1.419632126,0.319206404 };
						//constexpr double f_acc[6] = { 0,3.555679326,0.344454603,0.148247716,0.048552673,0.033815455 };
						//constexpr double f2c_index[6] = { 9.07327526291993, 9.07327526291993, 17.5690184835913, 39.0310903520972, 66.3992503259041, 107.566785527965 };
						//constexpr double f_static_index[6] = {0.5, 0.5, 0.5, 0.85, 0.95, 0.8};

						//��Ħ����+��Ħ����=ft_friction

						real_vel[i] = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
						ft_friction1[i] = 0.8*(f_static[i] * real_vel[i] / max_static_vel[i]);

						//double ft_friction2_max = std::max(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? f_static[i] - ft_friction1[i] : f_static[i] + ft_friction1[i]);
						//double ft_friction2_min = std::min(0.0, controller->motionAtAbs(i).actualVel() >= 0 ? -f_static[i] + ft_friction1[i] : -f_static[i] - ft_friction1[i]);
						//ft_friction2[i] = std::max(ft_friction2_min, std::min(ft_friction2_max, ft_friction2_index[i] * param.ft[i]));
						//ft_friction[i] = ft_friction1[i] + ft_friction2[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

						ft_friction[i] = ft_friction1[i] + f_vel[i] * controller->motionAtAbs(i).actualVel();

						//auto real_vel = std::max(std::min(max_static_vel[i], controller->motionAtAbs(i).actualVel()), -max_static_vel[i]);
						//ft_friction = (f_vel[i] * controller->motionAtAbs(i).actualVel() + f_static_index[i] * f_static[i] * real_vel / max_static_vel[i])*f2c_index[i];

						ft_friction[i] = std::max(-500.0, ft_friction[i]);
						ft_friction[i] = std::min(500.0, ft_friction[i]);

						//����ѧ�غ�=ft_dynamic
						ft_dynamic[i] = target.model->motionPool()[i].mfDyn();

						ft_offset[i] = (ft_friction[i] + ft_dynamic[i] + param.ft[i])*f2c_index[i];
						controller->motionAtAbs(i).setTargetCur(ft_offset[i]);
					}
				}
			}

			//print//
			auto &cout = controller->mout();
			if (target.count % 1000 == 0)
			{
				cout << "pt:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << param.pt[i] << "  ";
					}
				}
				cout << std::endl;

				cout << "pa:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << param.pa[i] << "  ";
					}
				}
				cout << std::endl;

				cout << "vt:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << param.vt[i] << "  ";
					}
				}
				cout << std::endl;

				cout << "va:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << param.va[i] << "  ";
					}
				}
				cout << std::endl;

				cout << "vproportion:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << vproportion[i] << "  ";
					}
				}
				cout << std::endl;

				cout << "vinteg:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << vinteg[i] << "  ";
					}
				}
				cout << std::endl;

				cout << "vdiff:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << vdiff[i] << "  ";
						cout << std::setw(10) << param.kd_v[i] << "  ";

					}
				}
				cout << std::endl;

				cout << "ft:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << param.ft[i] << "  ";
					}
				}
				cout << std::endl;
				cout << "friction1:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << ft_friction1[i] << "  ";
					}
				}
				cout << std::endl;
				cout << "friction:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << ft_friction[i] << "  ";
					}
				}
				cout << std::endl;
				cout << "ft_dynamic:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << ft_dynamic[i] << "  ";
					}
				}
				cout << std::endl;
				cout << "ft_offset:";
				for (Size i = 0; i < 6; i++)
				{
					if (param.joint_active_vec[i])
					{
						cout << std::setw(10) << ft_offset[i] << "  ";
					}
				}
				cout << std::endl;

				cout << "------------------------------------------------" << std::endl;
			}

			// log //
			auto &lout = controller->lout();
			for (Size i = 0; i < param.ft.size(); i++)
			{
				lout << param.pt[i] << ",";
				lout << controller->motionAtAbs(i).actualPos() << ",";
				lout << param.vt[i] << ",";
				lout << controller->motionAtAbs(i).actualVel() << ",";
				lout << ft_offset[i] << ",";
				lout << controller->motionAtAbs(i).actualCur() << ",";
				lout << vproportion[i] << ",";
				lout << vinteg[i] << ",";
				lout << vdiff[i] << ",";
				lout << param.ft[i] << ",";
				lout << ft_friction1[i] << ",";
				lout << ft_friction[i] << ",";
				lout << ft_dynamic[i] << ",";
			}
			lout << std::endl;

			return (!is_running&&ds_is_all_finished&&md_is_all_finished) ? 0 : 1;
		}
	auto MoveJPID::collectNrt(PlanTarget &target)->void {}
	MoveJPID::MoveJPID(const std::string &name) :Plan(name)
		{
			command().loadXmlStr(
				"<moveJPID>"
				"	<group type=\"GroupParam\" default_child_type=\"Param\">"
				"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"all\">"
				"			<all abbreviation=\"a\"/>"
				"			<motion_id abbreviation=\"m\" default=\"0\"/>"
				"			<physical_id abbreviation=\"p\" default=\"0\"/>"
				"			<slave_id abbreviation=\"s\" default=\"0\"/>"
				"		</unique>"
				"		<pos default=\"0.0\"/>"
				"		<kp_p default=\"{10,12,70,5,6,3}\"/>"
				"		<kp_v default=\"{270,360,120,120,60,25}\"/>"
				"		<ki_v default=\"{2,18,20.0,0.6,0.5,0.5}\"/>"
				"		<kd_v default=\"0\"/>"
				"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_all\">"
				"			<check_all/>"
				"			<check_none/>"
				"			<group type=\"GroupParam\" default_child_type=\"Param\">"
				"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos\">"
				"					<check_pos/>"
				"					<not_check_pos/>"
				"					<group type=\"GroupParam\" default_child_type=\"Param\">"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_max\">"
				"							<check_pos_max/>"
				"							<not_check_pos_max/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_min\">"
				"							<check_pos_min/>"
				"							<not_check_pos_min/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous\">"
				"							<check_pos_continuous/>"
				"							<not_check_pos_continuous/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_at_start\">"
				"							<check_pos_continuous_at_start/>"
				"							<not_check_pos_continuous_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order\">"
				"							<check_pos_continuous_second_order/>"
				"							<not_check_pos_continuous_second_order/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_continuous_second_order_at_start\">"
				"							<check_pos_continuous_second_order_at_start/>"
				"							<not_check_pos_continuous_second_order_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_pos_following_error\">"
				"							<check_pos_following_error/>"
				"							<not_check_pos_following_error />"
				"						</unique>"
				"					</group>"
				"				</unique>"
				"				<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel\">"
				"					<check_vel/>"
				"					<not_check_vel/>"
				"					<group type=\"GroupParam\" default_child_type=\"Param\">"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_max\">"
				"							<check_vel_max/>"
				"							<not_check_vel_max/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_min\">"
				"							<check_vel_min/>"
				"							<not_check_vel_min/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous\">"
				"							<check_vel_continuous/>"
				"							<not_check_vel_continuous/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_continuous_at_start\">"
				"							<check_vel_continuous_at_start/>"
				"							<not_check_vel_continuous_at_start/>"
				"						</unique>"
				"						<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"check_vel_following_error\">"
				"							<check_vel_following_error/>"
				"							<not_check_vel_following_error />"
				"						</unique>"
				"					</group>"
				"				</unique>"
				"			</group>"
				"		</unique>"
				"	</group>"
				"</moveJPID>");
		}


	// ����ָֹͣ���ֹͣMoveStop��ȥʹ�ܵ�� //
	auto MoveStop::prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
		{
			enable_moveJRC = false;
			enable_movePQCrash = false;
			enable_moveJCrash = false;
			enable_moveJF = false;
			enable_moveJFB = false;
			enable_moveJPID = false;
			target.option = aris::plan::Plan::NOT_RUN_EXECUTE_FUNCTION;
		}
	MoveStop::MoveStop(const std::string &name) :Plan(name)
		{
			command().loadXmlStr(
				"<moveStop>"
				"</moveStop>");
		}
	

	// ���ظ���ָ���ʵʱ����ĩ��pqֵ //
	auto MoveSPQ::prepairNrt(const std::map<std::string, std::string> &params, PlanTarget &target)->void
	{
		for (auto &p : params)
		{
			if (p.first == "setpqJF")
			{
				auto pqarray = target.model->calculator().calculateExpression(p.second);
				std::array<double, 7> temp;
				std::copy(pqarray.begin(), pqarray.end(), temp.begin());
				setpqJF.store(temp);
			}
			else if (p.first == "setpqJFB")
			{
				auto pqarray = target.model->calculator().calculateExpression(p.second);
				std::array<double, 7> temp;
				std::copy(pqarray.begin(), pqarray.end(), temp.begin());
				setpqJFB.store(temp);
			}
		}
		target.option = aris::plan::Plan::NOT_RUN_EXECUTE_FUNCTION;
	}
	MoveSPQ::MoveSPQ(const std::string &name) :Plan(name)
	{
		command().loadXmlStr(
			"<moveSPQ>"
			"	<group type=\"GroupParam\" default_child_type=\"Param\">"
			"		<unique type=\"UniqueParam\" default_child_type=\"Param\" default=\"setpqJFB\">"
			"			<setpqJF default=\"{0.42,0.0,0.55,0,0,0,1}\"/>"
			"			<setpqJFB default=\"{0.42,0.0,0.55,0,0,0,1}\"/>"
			"		</unique>"
			"	</group>"
			"</moveSPQ>");
	}

}
