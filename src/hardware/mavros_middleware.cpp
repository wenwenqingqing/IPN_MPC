#include <hardware/mavros_middleware.h>

namespace middleware
{
    MavrosMiddleware::MavrosMiddleware(const ros::NodeHandle &nh, int takeoff_alt) 
        : nh_(nh), takeoff_altitude_(takeoff_alt)
    {        
        arming_client_   = nh_.serviceClient<mavros_msgs::CommandBool  >("mavros/cmd/arming");
        land_client_     = nh_.serviceClient<mavros_msgs::CommandTOL   >("mavros/cmd/land");
        local_pos_pub_   = nh_.advertise    <geometry_msgs::PoseStamped>("mavros/setpoint_position/local", 10);
        set_mode_client_ = nh_.serviceClient<mavros_msgs::SetMode      >("mavros/set_mode");
        state_sub_       = nh_.subscribe    <mavros_msgs::State        >("mavros/state", 10, &MavrosMiddleware::MavstateCb, this, ros::TransportHints().tcpNoDelay());
        ctrl_input_sub_  = nh_.subscribe    <IPN_MPC::INPUT            >("internal_ctrl_input", 100, &CtrlInputCb, this, ros::TransportHints().tcpNoDelay());
    }

    bool MavrosMiddleware::Connect()
    {
        ros::Rate rate(100);
        // wait for FCU connection
        while(ros::ok() && current_state_.connected)
        {
            ros::spinOnce();
            rate.sleep();
            ROS_INFO("Connecting to FCT...");
        }

        geometry_msgs::PoseStamped control;
        control.pose.position.x = 0;
        control.pose.position.y = 0;
        control.pose.position.z = takeoff_altitude_;

        ROS_INFO("Send a few setpoints before starting");

        for(int i = 100u; ros::ok() && i > 0; --i)
        {
            // execute motion 
            local_pos_pub_.publish(control);
            ros::spinOnce();
            rate.sleep();
        }

        return true;
    }

    bool MavrosMiddleware::OffboardHover()
    {
        ros::Rate rate(100);

        geometry_msgs::PoseStamped control;
        control.pose.position.x = 0;
        control.pose.position.y = 0;
        control.pose.position.z = takeoff_altitude_;

        mavros_msgs::SetMode offb_set_mode;
        offb_set_mode.request.custom_mode = "OFFBOARD";

        mavros_msgs::CommandBool arm_cmd;
        arm_cmd.request.value = true;

        mavros_msgs::CommandTOL land_cmd;
        land_cmd.request.yaw       = 0;
        land_cmd.request.latitude  = 0;
        land_cmd.request.longitude = 0;
        land_cmd.request.altitude  = 0; 

        ros::Time last_request = ros::Time::now();

        ROS_INFO("Change to offboard mode and arm");
        ROS_INFO("Armed -> %s\n", current_state_.mode.c_str());

        while(ros::ok() && !current_state_.armed)
        {
            if( current_state_.mode != "OFFBOARD" && (ros::Time::now() - last_request > ros::Duration(5.0)))
            {
                ROS_INFO("%s\n", current_state_.mode.c_str());
                if( set_mode_client_.call(offb_set_mode) && offb_set_mode.response.mode_sent)
                {
                    ROS_INFO("Offboard enabled");
                }
                last_request = ros::Time::now();
            } 
            else 
            {
                if(!current_state_.armed && (ros::Time::now() - last_request > ros::Duration(5.0)))
                {
                    if(arming_client_.call(arm_cmd) && arm_cmd.response.success)
                    {
                        ROS_INFO("Vehicle armed");
                    }
                    else
                    {
                        std::cout << "Try to arm";
                    }
                    last_request = ros::Time::now();
                }
            }
            
            // Go to hover point 
            local_pos_pub_.publish(control);
            ros::spinOnce();
            rate.sleep();
        }

        return true;
    }

    void MavrosMiddleware::EstThrustAccRatio()
    {

    }

    // 100hz control loop
    void MavrosMiddleware::Loop()
    {
        ros::Rate rate(ctrl_freq_);

        while(ros::ok())
        {
            switch (internal_ctrl_input_.control_mode)
            {
            case Ctrl_mode::pose:
                break;

            case Ctrl_mode::att_thrust:
                break;

            case Ctrl_mode::bodyrate_acc:
                break;

            case Ctrl_mode::bodyrate_thrust:
                break;
            
            case Ctrl_mode::none:
                break;

            default:
                break;
            }

            internal_ctrl_input_.control_mode = Ctrl_mode::none;
            
            EstThrustAccRatio();
            
            ros::spinOnce();
            rate.sleep();
        }
    }


} // namespace middleware

