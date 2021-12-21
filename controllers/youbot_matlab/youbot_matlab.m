% Description: MATLAB controller example for Webots

% uncomment the next two lines if you want to use
% MATLAB's desktop and interact with the controller
%desktop;
%keyboard;

arm_init
base_init
gripper_init
passive_wait(2.2)
gripper_release()
arm_set_height('ARM_FRONT_CARDBOARD_BOX')
passive_wait(4.0)
gripper_grip
passive_wait(1.0)
arm_set_height('ARM_BACK_PLATE_LOW')
passive_wait(3.0)
gripper_release()
passive_wait(1.0)
arm_perform_reset()
base_strafe_left()
passive_wait(5.0)
gripper_grip
base_reset
passive_wait(1.0)
base_turn_left
passive_wait(1.0)
base_reset
gripper_release
arm_set_height('ARM_BACK_PLATE_LOW')
passive_wait(3.0)
gripper_grip
passive_wait(1.0)
arm_set_height('ARM_RESET')
passive_wait(2.0)
arm_set_height('ARM_FRONT_PLATE')
arm_set_orientation('ARM_RIGHT')
passive_wait(4.0)
arm_set_height('ARM_FRONT_FLOOR')
passive_wait(2.0)
gripper_release
passive_wait(1.0)
arm_set_height('ARM_FRONT_PLATE')
passive_wait(2.0)
arm_set_height('ARM_RESET')
passive_wait(2.0)
arm_perform_reset
gripper_grip
passive_wait(2.0)
quit
