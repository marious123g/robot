#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <base.h>
#include <gripper.h>
#include <webots/keyboard.h>
#include <webots/robot.h>
#include <webots/motor.h>
#include <webots/touch_sensor.h>
#include <webots/camera.h>
#include <webots/camera_recognition_object.h>
#include <webots/gps.h>
#include <webots/compass.h>
#define TIME_STEP 32

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) > (b)) ? (b) : (a))
#define abs(a) (((a) < (0)) ? (0) : (a))
#define MAX_WIDTH 0.2f
#define MIN_WIDTH 0.0f
// WbDeviceTag motorL;
// WbDeviceTag motorR;
WbDeviceTag forceL;
WbDeviceTag forceR;
#define MAX_HEIGHT 0.4f
#define MIN_HEIGHT 0.03f
// WbDeviceTag motorM;
#define GRIPPER_MOTOR_MAX_SPEED 0.1
#define PI 3.1415926535f
static WbDeviceTag gripper_motors[3];
static WbDeviceTag camera[2];
WbDeviceTag gps;
WbDeviceTag compass;
double gps_values[2];         //gpsֵ
double compass_angle;         //���̽Ƕ�
double initial_posture[3];    //���λ��,0Ϊx,1Ϊz,2Ϊ�Ƕȣ�ÿ�ι켣ֻ����һ��
double tmp_target_posture[3]; //��ʱĿ��λ�ˣ���Ҫ���ϼ���
double fin_target_posture[3]; //����Ŀ��λ��


int TargetIndex = 0;          //��ǰ��ע�Ļ��ܿ�λ
int TargetGood;               //��ǰ��ע�Ļ�������
int Item_Grasped_Id = -1;
double load_target_posture[3];//�ϻ���

char* GoodsList[] = { "can", "cereal box", "cereal box red", "jam jar", "honey jar", "water bottle", "biscuit box", "red can", "beer bottle" };
//ץȡʱǰ̽�ľ��룬����ֵԽС��ǰ̽Խǰ
double Grasp_dis_set[] = { -0.16,  -0.18,         -0.18,             -0.16,       -0.16,       -0.16,         -0.16,    -0.16,       -0.16 };
//Ѱ�һ��ﶨ�� ��->...-> ��->...->��->...->��
int Travel_Point_Index = 0; //������
int travel_points_sum = 0;//�߹��Ķ�������
double fixed_posture_travelaround[12][3] = {
        {1.05, 0.00, PI * 2},      //��
        {1.05, -1.05, PI * 2},     //����
        {1.05, -1.05, PI / 2},     //���� ת
        {0.00, -1.05, PI / 2},     //��
        {-1.05, -1.05, PI / 2},    //����
        {-1.05, -1.05, PI},        //���� ת
        {-1.05, 0, PI},            //��
        {-1.05, 1.05, PI},         //����
        {-1.05, 1.05, 3 * PI / 2}, //���� ת
        {0.00, 1.05, 3 * PI / 2},  //��
        {1.05, 1.05, 3 * PI / 2},  //����
        {1.05, 1.05, PI * 2}       //���� ת
};
//ʶ��ջ��ܶ��� ��->��->��->��
int CurrentShelf = 0;         //��ǰ���ܱ�� ������ ��ʱ��
double fixed_posture_findempty[4][3] =
{
  {1.05, 0.00, 0},          //��
  {0.00, -1.05, PI / 2},    //��
  {-1.05, 0, PI},           //��
  {0.00, 1.05, 3 * PI / 2} //��
};
double fixed_posture_loadItem[4][3] =
{
  {1.05, 0.00, PI},          //��
  {0.00, -1.05, 3 * PI / 2}, //��
  {-1.05, 0, 0},             //��
  {0.00, 1.05, PI / 2}      //��
};


//������״̬���
enum RobotState
{
    Init_Pose,
    Recognize_Empty,
    Arround_Moving,
    Grab_Item,
    Back_Moving,
    TurnBack_To_LoadItem,
    Item_Loading,
    RunTo_NextShelf
};

double width = 0.0;  //צ��0~0.1
double height = 0.0; //צ��-0.05~0.45

static void step();
static void passive_wait(double sec);
static void display_helper_message();
void lift(double position);
void moveFingers(double position);
void init_all();
void caculate_tmp_target(double tmp_posture[], double fin_posture[]);
void set_posture(double posture[], double x, double z, double angle);
void get_gps_values(double v_gps[]);
double vector2_angle(const double v1[], const double v2[]);
void get_compass_angle(double* ret_angle);
int keyboard_control(int c, int* main_state);
bool targetdist_reached(double target_posture[], double dist_threshold);
bool targetpos_reached(double target_posture[], double pos_threshold);
int nameToIndex(char* name);
char* indexToName(int index);

bool Find_Empty(WbDeviceTag camera);
bool Find_Goods(WbDeviceTag camera, char* good_name, int* item_grasped_id);
bool Aim_and_Grasp(int* grasp_state, WbDeviceTag camera, int objectID);
bool Moveto_CertainPoint(double fin_posture[], double reach_precision);
void Robot_State_Machine(int* main_state, int* grasp_state);

//*?                 main����      <��ʼ>            ?*//
//������

int main(int argc, char** argv)
{
    init_all();

    printf("Ready to go!\n");
    int main_state = 0;  //����������״̬
    int grasp_state = 0; //��צ״̬
    while (true)
    {
        step();
        Robot_State_Machine(&main_state, &grasp_state);
        // printf("State:%d\n", main_state);
        keyboard_control(wb_keyboard_get_key(), &main_state);
    }

    wb_robot_cleanup();

    return 0;
}
//*?                 main����       <����>            ?*//

//*?                 ���Ŀ��ƺ���    <��ʼ>            ?*//
//��ģ���ʼ��
void init_all()
{
    // �����˳�ʼ��
    wb_robot_init();
    base_init();
    passive_wait(2.0);

    camera[0] = wb_robot_get_device("camera_back"); //�����ʼ��
    camera[1] = wb_robot_get_device("camera_front");
    wb_camera_enable(camera[0], TIME_STEP);
    wb_camera_recognition_enable(camera[0], TIME_STEP);
    wb_camera_enable(camera[1], TIME_STEP);
    wb_camera_recognition_enable(camera[1], TIME_STEP);

    //GPS��ʼ��
    gps = wb_robot_get_device("gps_copy");
    wb_gps_enable(gps, TIME_STEP);
    //Compass��ʼ��
    compass = wb_robot_get_device("compass_copy");
    wb_compass_enable(compass, TIME_STEP);
    //����ȫ��λ�ƶ���ʼ��
    base_goto_init(TIME_STEP);
    //���ó�ʼλ��
    step();
    get_gps_values(gps_values);
    get_compass_angle(&compass_angle);
    set_posture(initial_posture, gps_values[0], gps_values[1], compass_angle);
    //���õ�һ������λ��
    set_posture(fin_target_posture, fixed_posture_findempty[CurrentShelf][0], fixed_posture_findempty[CurrentShelf][1], fixed_posture_findempty[CurrentShelf][2]);
    //������һ����ʱĿ��;
    caculate_tmp_target(tmp_target_posture, fin_target_posture);
    //���õ����˶�Ŀ��
    base_goto_set_target(tmp_target_posture[0], tmp_target_posture[1], tmp_target_posture[2]);

    display_helper_message();
    wb_keyboard_enable(TIME_STEP);

    gripper_motors[0] = wb_robot_get_device("lift motor");
    gripper_motors[1] = wb_robot_get_device("left finger motor");
    gripper_motors[2] = wb_robot_get_device("right finger motor");

    //�����������
    wb_motor_enable_force_feedback(gripper_motors[1], 1);
    wb_motor_enable_force_feedback(gripper_motors[2], 1);


}

//������״̬��
void Robot_State_Machine(int* main_state, int* grasp_state)
{
    switch (*main_state)
    {
        //��ʼ����״̬��վ���ĸ�����֮һ��׼��ʶ��ջ���
    case Init_Pose:
    {
        //CurrentShelf = 0;
        if (Moveto_CertainPoint(fin_target_posture, 0.01))
        {
            *main_state = Recognize_Empty;
            printf("״̬�ı䣺 Init_Pose to Recognize_Empty!\n");
        }
        break;
    }
    //ʶ��ջ���
    case Recognize_Empty:
    {
        if (Find_Empty(camera[0])) //�������п�λ
        {
            *main_state = Arround_Moving;
            printf("״̬�ı䣺 Recognize_Empty to Arround_Moving!\n");
            set_posture(initial_posture, gps_values[0], gps_values[1], compass_angle);
            set_posture(fin_target_posture, fixed_posture_travelaround[Travel_Point_Index][0], fixed_posture_travelaround[Travel_Point_Index][1], fixed_posture_travelaround[Travel_Point_Index][2]);
        }
        else //�������޿�λ
        {
            printf("�������%d���ò�����~\n", CurrentShelf);
            CurrentShelf += 1;//������һ����,
            CurrentShelf %= 4;
            Travel_Point_Index += 1;
            Travel_Point_Index %= 12;
            set_posture(initial_posture, gps_values[0], gps_values[1], compass_angle);
            set_posture(fin_target_posture, fixed_posture_travelaround[Travel_Point_Index][0], fixed_posture_travelaround[Travel_Point_Index][1], fixed_posture_travelaround[Travel_Point_Index][2]);
            *main_state = RunTo_NextShelf;//ǰ����һ������
            printf("״̬�ı䣺 Recognize_Empty to RunTo_NextShelf!\n");
        }
        break;
    }
    //�������˶�
    case Arround_Moving:
    {
        //����дʶ���ץȡ���� �����������ʱ��Ʒ
        if (Find_Goods(camera[1], indexToName(TargetGood), &Item_Grasped_Id))
        {
            *main_state = Grab_Item;
            printf("״̬�ı䣺 Arround_Moving to Grab_Item!\n");
            set_posture(fin_target_posture, gps_values[0], gps_values[1], compass_angle);
        }
        else
        {
            if (Moveto_CertainPoint(fin_target_posture, 0.05))
            {
                travel_points_sum++;
                printf("GoodsonShelf[%d][%d] need %s\n", CurrentShelf, TargetIndex, indexToName(TargetGood));
                set_posture(initial_posture, gps_values[0], gps_values[1], compass_angle);
                Travel_Point_Index += 1;
                Travel_Point_Index %= 12;
                set_posture(fin_target_posture, fixed_posture_travelaround[Travel_Point_Index][0], fixed_posture_travelaround[Travel_Point_Index][1], fixed_posture_travelaround[Travel_Point_Index][2]);
            }
        }
        break;
    }
    //ץ��Ʒ
    case Grab_Item:
    {
        if (Aim_and_Grasp(grasp_state, camera[1], Item_Grasped_Id))
        {
            printf("ץ����ȥ����\n");
            *main_state = Back_Moving;
            printf("״̬�ı䣺 Grab_Item to Back_Moving!\n");
            *grasp_state = 0;
        }
        break;
    }
    //ȡ���س�
    case Back_Moving:
    {
        //�������ı߽���Ȧ������
        if (Moveto_CertainPoint(fin_target_posture, 0.05))
        {
            if (fin_target_posture[0] == fixed_posture_findempty[CurrentShelf][0] && fin_target_posture[1] == fixed_posture_findempty[CurrentShelf][1])
            {
                *main_state = TurnBack_To_LoadItem;
                printf("״̬�ı䣺 Back_Moving to TurnBack_To_LoadItem!\n");
                set_posture(initial_posture, gps_values[0], gps_values[1], compass_angle);
                //������һĿ���Ϊ����180��
                set_posture(fin_target_posture, fixed_posture_loadItem[CurrentShelf][0], fixed_posture_loadItem[CurrentShelf][1], fixed_posture_loadItem[CurrentShelf][2]);
                printf("���ϻ��ĵط��ˣ�\n");
                travel_points_sum = 0;
            }
            else
            {
                set_posture(initial_posture, gps_values[0], gps_values[1], compass_angle);
                if (travel_points_sum >= 6) Travel_Point_Index += 1;//˳ʱ��ת
                else
                {
                    if (Travel_Point_Index == 0) Travel_Point_Index = 12;
                    Travel_Point_Index -= 1;//��ʱ��ת
                }
                Travel_Point_Index %= 12;
                Travel_Point_Index = max(0, Travel_Point_Index);
                set_posture(fin_target_posture, fixed_posture_travelaround[Travel_Point_Index][0], fixed_posture_travelaround[Travel_Point_Index][1], fixed_posture_travelaround[Travel_Point_Index][2]);
            }

        }
        break;
    }
    //ת��׼���ϻ�����
    case TurnBack_To_LoadItem:
    {
        if (Moveto_CertainPoint(fin_target_posture, 0.03))
        {
            *main_state = Item_Loading;
            printf("״̬�ı䣺 TurnBack_To_LoadItem to Item_Loading!\n");
            printf("���ֿ�ȱ��\n");
            printf("GoodsonShelf[%d][%d] need %s\n", CurrentShelf, TargetIndex, indexToName(TargetGood));

            //����һ�κ��ϻ�������λ��
            get_gps_values(gps_values);
            get_compass_angle(&compass_angle);
            double load_x = (TargetIndex % 8) * 0.24 - 0.84;
            double load_z = -0.16;//������
            load_target_posture[0] = gps_values[0] - sin(compass_angle) * load_x + cos(compass_angle) * load_z;
            load_target_posture[1] = gps_values[1] - cos(compass_angle) * load_x - sin(compass_angle) * load_z;
            load_target_posture[2] = compass_angle;
        }
        break;
    }
    //�ϻ�
    case Item_Loading:
    {
        get_gps_values(gps_values);
        // printf("GPS device: %.3f %.3f\n", gps_values[0], gps_values[1]);
        if (Moveto_CertainPoint(load_target_posture, 0.001))
        {
            double load_z_add = -0.24;//���ǰ��һЩ
            load_target_posture[0] = gps_values[0] + cos(compass_angle) * load_z_add;
            load_target_posture[1] = gps_values[1] - sin(compass_angle) * load_z_add;
            load_target_posture[2] = compass_angle;
            while (!Moveto_CertainPoint(load_target_posture, 0.01))
            {
                step();//��������ʱ�� ֱ��������ѭ����
            }
            base_reset();
            printf("С���ϻ���\n");
            wb_robot_step(50000 / TIME_STEP);
            moveFingers(width += 0.005);
            wb_robot_step(50000 / TIME_STEP);

            load_target_posture[0] = gps_values[0] - cos(compass_angle) * load_z_add;
            load_target_posture[1] = gps_values[1] + sin(compass_angle) * load_z_add;
            load_target_posture[2] = compass_angle;
            while (!Moveto_CertainPoint(load_target_posture, 0.01))
            {
                step(); //��������ʱ�� ֱ��������ѭ����
            }
            load_target_posture[2] = (compass_angle > PI) ? compass_angle - PI : compass_angle + PI; //ԭ����ת����

            while (!Moveto_CertainPoint(load_target_posture, 0.01))
            {
                step(); //��������ʱ�� ֱ��������ѭ����
            }
            moveFingers(width = 0.0);
            lift(height = 0.020);
            *main_state = Init_Pose;
            printf("״̬�ı䣺 Item_Loading to Init_Pose!\n");
            set_posture(initial_posture, gps_values[0], gps_values[1], compass_angle);
            set_posture(fin_target_posture, fixed_posture_findempty[CurrentShelf][0], fixed_posture_findempty[CurrentShelf][1], fixed_posture_findempty[CurrentShelf][2]);
        }
        break;
    }
    case RunTo_NextShelf:
    {
        if (Moveto_CertainPoint(fin_target_posture, 0.05))
        {
            //������һ������
            if (fin_target_posture[0] == fixed_posture_findempty[CurrentShelf][0] && fin_target_posture[1] == fixed_posture_findempty[CurrentShelf][1])
            {
                printf("�����¸�����%d\n", CurrentShelf);
                *main_state = Recognize_Empty;
                printf("״̬�ı䣺 RunTo_NextShelf to Recognize_Empty!\n");
                printf("initial target�� %.3f  %.3f  %.3f\n", initial_posture[0], initial_posture[1], initial_posture[2]);
                printf("tmp target�� %.3f  %.3f  %.3f\n", tmp_target_posture[0], tmp_target_posture[1], tmp_target_posture[2]);
                printf("final target�� %.3f  %.3f  %.3f\n\n", fin_target_posture[0], fin_target_posture[1], fin_target_posture[2]);
            }
            else
            {
                set_posture(initial_posture, gps_values[0], gps_values[1], compass_angle);
                Travel_Point_Index += 1;
                Travel_Point_Index %= 12;
                set_posture(fin_target_posture, fixed_posture_travelaround[Travel_Point_Index][0], fixed_posture_travelaround[Travel_Point_Index][1], fixed_posture_travelaround[Travel_Point_Index][2]);
            }
        }

    }
    //ERROR
    default:
    {
        // printf("Error form State Machine : %d\n",*main_state);
        break;
    }
    }
}

//���̿��ƻ����˶�
int keyboard_control(int c, int* main_state)
{
    if ((c >= 0))
    { //&& c != pc) {//��Ҫ���ֵ�仯
        switch (c)
        {
        case 'G':
        {
            get_gps_values(gps_values);
            // printf("GPS device: %.3f %.3f\n", gps_values[0], gps_values[1]);
            get_compass_angle(&compass_angle);
            printf("Compass device: %.3f\n", compass_angle);
            break;
        }
        case 'C':
        {
            printf("Manually stopped��\n");
            *main_state = -1;
            break;
        }
        case WB_KEYBOARD_UP:
            printf("Go forwards\n");
            base_forwards();
            break;
        case WB_KEYBOARD_DOWN:
            printf("Go backwards\n");
            base_backwards();
            break;
        case WB_KEYBOARD_LEFT:
            printf("Strafe left\n");
            base_strafe_left();
            break;
        case WB_KEYBOARD_RIGHT:
            printf("Strafe right\n");
            base_strafe_right();
            break;
        case WB_KEYBOARD_PAGEUP:
            printf("Turn left\n");
            base_turn_left();
            break;
        case WB_KEYBOARD_PAGEDOWN:
            printf("Turn right\n");
            base_turn_right();
            break;
        case WB_KEYBOARD_END:
        case ' ':
            printf("Reset\n");
            base_reset();
            // arm_reset();
            break;
        case '+':
        case 388:
        case 65585:
            printf("Grip\n");
            //  gripper_grip();
            break;
        case '-':
        case 390:
            printf("Ungrip\n");
            //  gripper_release();
            break;
        case 332:
        case WB_KEYBOARD_UP | WB_KEYBOARD_SHIFT:
            //UpDownControll(Target_Height+=0.02);
            lift(height += 0.005);
            printf("Increase arm height to %.3f\n", height);
            break;
        case 326:
        case WB_KEYBOARD_DOWN | WB_KEYBOARD_SHIFT:
            //UpDownControll(Target_Height-=0.02);
            lift(height -= 0.005);
            printf("Decrease arm height to %.3f\n", height);
            // arm_decrease_height();
            break;
        case 330:
        case WB_KEYBOARD_RIGHT | WB_KEYBOARD_SHIFT:
            //ClawControll(Target_Width-=0.01);
            moveFingers(width -= 0.001);
            printf("Close the Claws to %.3f\n", width);
            break;
        case 328:
        case WB_KEYBOARD_LEFT | WB_KEYBOARD_SHIFT:
            //ClawControll(Target_Width+=0.01);
            moveFingers(width += 0.001);
            printf("Open the Claws to %.3f\n", width);
            break;
        default:
            fprintf(stderr, "Wrong keyboard input\n");
            break;
        }
    }
    return 0;
}

//GPS�˶���ָ��λ�ˣ�����boolֵ�����Ƿ񵽴Ĭ�Ͼ���0.05
bool Moveto_CertainPoint(double fin_posture[], double reach_precision)
{
    if (targetdist_reached(fin_posture, reach_precision) && targetpos_reached(fin_posture, reach_precision))
    {
        // printf("����Ŀ��λ�ã�\n");
        // base_reset();
        return true;
    }
    else
    {
        caculate_tmp_target(tmp_target_posture, fin_posture);
        base_goto_set_target(tmp_target_posture[0], tmp_target_posture[1], tmp_target_posture[2]);
        base_goto_run();
        return false;
    }
}

//ǰ������ͷУ׼��ץȡ
bool Aim_and_Grasp(int* grasp_state, WbDeviceTag camera, int objectID)
{
    //���ɺ�ID43 ˮƿID56
    int number_of_objects = wb_camera_recognition_get_number_of_objects(camera);
    const WbCameraRecognitionObject* objects = wb_camera_recognition_get_objects(camera);
    for (int i = 0; i < number_of_objects; ++i)
    {
        if (objects[i].id == objectID) //�ҵ������е�һ��ID����
        {
            if (*grasp_state == 0) //����λ��
            {
                //��Ƭ�������ץȡ��
                if (!strcmp("cereal box red", objects[i].model) || !strcmp("cereal box", objects[i].model))
                    lift(height = 0.05);
                //ˮƿ��Ҫ���ץȡ�㣬���ǵ�����Ȼ��
                else if (!strcmp("water bottle", objects[i].model))
                    lift(height = 0.10);
                else lift(height = 0.0);
                moveFingers(width = objects[i].size[0] / 1.5);
                // printf("ID %d ������ %s �� %lf %lf\n", objects[i].id, objects[i].model, objects[i].position[0], objects[i].position[2]);
                get_gps_values(gps_values);
                get_compass_angle(&compass_angle);
                double grasp_target_posture[3];

                double grasp_dis_set = Grasp_dis_set[nameToIndex(objects[i].model)];
                // printf("ץȡ����:%.3f\n",grasp_dis_set);
                //���ƫ�� ͬʱ����λ����΢����һ��
                grasp_target_posture[0] = gps_values[0] - sin(compass_angle) * objects[i].position[0] + cos(compass_angle) * (objects[i].position[2] - grasp_dis_set) * 0.6;
                grasp_target_posture[1] = gps_values[1] - cos(compass_angle) * objects[i].position[0] - sin(compass_angle) * (objects[i].position[2] - grasp_dis_set) * 0.6;
                grasp_target_posture[2] = compass_angle;

                Moveto_CertainPoint(grasp_target_posture, 0.05);

                double grasp_threshold = 0.005;
                if (fabs(objects[i].position[0]) < grasp_threshold && fabs(objects[i].position[2] - grasp_dis_set) < grasp_threshold)
                {
                    *grasp_state += 1;
                    printf("��׼�ˣ�\n");
                    base_reset();
                    // ���Ӿ�������ץ�ֻ���ֵ
                    printf("�����С: %lf %lf\n", objects[i].size[0], objects[i].size[1]);
                    moveFingers(width = objects[i].size[0] / 2);
                    wb_robot_step(30000 / TIME_STEP);
                }
            }
            else if (*grasp_state == 1) //ץ
            {
                double grasp_force_threshold = 50.0;
                if (wb_motor_get_force_feedback(gripper_motors[1]) > -grasp_force_threshold)
                    moveFingers(width -= 0.0003); //����
                else
                {
                    printf("��ǰ�����������%.3f\n", wb_motor_get_force_feedback(gripper_motors[1]));
                    printf("��ץס��Ʒ\n");
                    wb_robot_step(30000 / TIME_STEP); //����ץ�ȶ�
                    if (wb_motor_get_force_feedback(gripper_motors[1]) <= -grasp_force_threshold)
                    {
                        printf("צ��%.4f\n", width);
                        *grasp_state += 1;
                        printf("GoodsonShelf[%d][%d] need %s\n", CurrentShelf, TargetIndex, indexToName(TargetGood));
                        if (!strcmp("water bottle", objects[i].model))//ˮ������߶�
                            lift(height = 0.12);
                        else if (!strcmp("cereal box red", objects[i].model))//����������߶�
                            lift(height = 0.50);
                        else if (!strcmp("cereal box", objects[i].model))
                            lift(height = 0.30);
                        else if (TargetIndex < 8)
                            lift(height = 0.05);
                        else if (CurrentShelf % 2 == 0) //����0.23
                            lift(height = 0.23);
                        else if (CurrentShelf % 2 == 1) //�߹�0.43
                            lift(height = 0.43);
                        printf("�����Ʒ��height = %.3f\n", height);
                        wb_robot_step(10000 / TIME_STEP);
                    }
                }
            }
            else if (*grasp_state == 2) //��
            {
                return true;
            }
            break;
        }
    }
    return false;
}

//Ѱ�ҿջ��� ���ĸ�����GPS ����ͷ������ǽ ���ػ���λ�ú�һ����Ʒ����
bool Find_Empty(WbDeviceTag camera)
{
    int number_of_objects = wb_camera_recognition_get_number_of_objects(camera);
    const WbCameraRecognitionObject* objects = wb_camera_recognition_get_objects(camera);
    int GoodsonShelf[4][16];      //�����ϵ���ƷID�� ���º��� �������
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 16; j++)
            GoodsonShelf[i][j] = -1;//��֪��Ϊɶд�ڶ����ʱ�򲻳ɹ�
    for (int i = 0; i < number_of_objects; ++i)
    {
        int Shelfx = max(0, floor((objects[i].position[0] + 0.84) * 4.17 + 0.5)); //���� ƽ�����0.24�����ӿ��0.25�����ƺ��Ӧһ��ϵ�� ��������
        int Shelfy = (objects[i].position[1] < -0.2) ? 0 : 1;             //���²� -0.20  Ϊ���·ֽ�

        GoodsonShelf[CurrentShelf][Shelfy * 8 + Shelfx] = nameToIndex(objects[i].model);
        printf("���� %s ��Ӧ��� %d д��[%d] д����Ϊ%d\n", objects[i].model, nameToIndex(objects[i].model), Shelfy * 8 + Shelfx, GoodsonShelf[CurrentShelf][Shelfy * 8 + Shelfx]);
    }

    //������ �ж���һ��Ҫȡ�Ļ�������
    int Empty_Flag = 0;

    for (int j = 0; j < 16; j++)
        printf("GoodsonShelf[%d][%d] = %d\n", CurrentShelf, j, GoodsonShelf[CurrentShelf][j]);
    for (int j = 0; j < 16; j++)
    {
        if (GoodsonShelf[CurrentShelf][j] == -1)
        {
            Empty_Flag = 1;
            TargetIndex = j;
            //Ѱ���ڽ����� �ж�Ӧ��ȡ�Ļ�������
            //ֱ�Ӹ��� ��װ�Ѿ�����ȥ��
            int TargetFloor = 0;
            if (j > 7)
                TargetFloor += 8; //�����޹�
            if (j % 8 < 4)
                for (int k = 0; k < 8; k++)//��������
                {
                    if (GoodsonShelf[CurrentShelf][TargetFloor + k] != -1)
                    {
                        // strcpy(TargetGood, GoodsonShelf[CurrentShelf][TargetFloor + k]);
                        TargetGood = GoodsonShelf[CurrentShelf][TargetFloor + k];
                        break;
                    }
                }
            else
                for (int k = 7; k >= 0; k--)//��������
                {
                    if (GoodsonShelf[CurrentShelf][TargetFloor + k] != -1)
                    {
                        // strcpy(TargetGood, GoodsonShelf[CurrentShelf][TargetFloor + k]);
                        TargetGood = GoodsonShelf[CurrentShelf][TargetFloor + k];
                        break;
                    }
                }
            //������Ŷ�û�п��ܻ���� �´�һ��
            printf("GoodsonShelf[%d][%d] need %s\n", CurrentShelf, j, indexToName(TargetGood));
            break;
        }
    }
    if (Empty_Flag)
        return true;
    else
        return false;
}

//��һ���̶���Ѳ�߹켣 ǰ������ͷѰ��ָ����Ʒ ����ֱ����������ͷ�ܲ�׽
bool Find_Goods(WbDeviceTag camera, char* good_name, int* item_grasped_id)
{
    int number_of_objects = wb_camera_recognition_get_number_of_objects(camera);
    const WbCameraRecognitionObject* objects = wb_camera_recognition_get_objects(camera);
    double grasp_dis_threshold = -0.5;
    for (int i = 0; i < number_of_objects; ++i)
    {
        if (strcmp(objects[i].model, good_name) == 0)
        {
            if (objects[i].position[2] > 1.3 * grasp_dis_threshold)
                // printf("���� %s �� %.3f m \n", good_name, -objects[i].position[2]);
              //�����������λ�öԡ����ǲ���
                if (objects[i].position[2] > grasp_dis_threshold && fabs(objects[i].position[0]) < 0.1 && objects[i].size[0] <= 0.15)
                {
                    printf("�ҵ�������%.3f m �� %s\n", -objects[i].position[2], good_name);
                    *item_grasped_id = objects[i].id;
                    return true;
                }
        }
    }
    return false;
}

// ǰ�� 1 step
static void step()
{
    if (wb_robot_step(TIME_STEP) == -1)
    {
        wb_robot_cleanup();
        exit(EXIT_SUCCESS);
    }
}

//���������ʱ
static void passive_wait(double sec)
{
    double start_time = wb_robot_get_time();
    do
    {
        step();
    } while (start_time + sec > wb_robot_get_time());
}

//��ӡ����
static void display_helper_message()
{
    printf("Control commands:\n");
    printf(" Arrows:       Move the robot\n");
    printf(" Page Up/Down: Rotate the robot\n");
    printf(" +/-:          (Un)grip\n");
    printf(" Shift + arrows:   Handle the arm\n");
    printf(" Space: Reset\n");
}

//���û�е�������߶�
void lift(double position)
{
    wb_motor_set_velocity(gripper_motors[0], GRIPPER_MOTOR_MAX_SPEED);
    wb_motor_set_position(gripper_motors[0], position);
}

//������צ���ϴ�С
void moveFingers(double position)
{
    wb_motor_set_velocity(gripper_motors[1], GRIPPER_MOTOR_MAX_SPEED);
    wb_motor_set_velocity(gripper_motors[2], GRIPPER_MOTOR_MAX_SPEED);
    wb_motor_set_position(gripper_motors[1], position);
    wb_motor_set_position(gripper_motors[2], position);
}

//ϸ��Ŀ��λ��
double SUB = 2.0; //ϸ��Ŀ�����
void caculate_tmp_target(double tmp_posture[], double fin_posture[])
{
    get_gps_values(gps_values);
    get_compass_angle(&compass_angle);
    tmp_posture[0] = gps_values[0] + (fin_posture[0] - gps_values[0]) / SUB;
    tmp_posture[1] = gps_values[1] + (fin_posture[1] - gps_values[1]) / SUB;
    //ѡ��������ת�Ƕ���С�ĵķ��������ת
    if (fabs(fin_posture[2] - compass_angle) > PI)
    {
        tmp_posture[2] = compass_angle + (compass_angle - fin_posture[2]) / (SUB * 5);
    }
    else
        tmp_posture[2] = compass_angle + (fin_posture[2] - compass_angle) / (SUB * 5);
}

//����λ��
void set_posture(double posture[], double x, double z, double angle)
{
    posture[0] = x;
    posture[1] = z;
    posture[2] = angle;
}

//bool���� �����Ƿ񵽴�ָ��λ��
bool targetdist_reached(double target_posture[], double dist_threshold)
{
    get_gps_values(gps_values);
    double dis = sqrt((gps_values[0] - target_posture[0]) * (gps_values[0] - target_posture[0]) + (gps_values[1] - target_posture[1]) * (gps_values[1] - target_posture[1]));

    // double angle = compass_angle - target_posture[2];
    if (dis <= dist_threshold)
    {
        return true;
    }
    else
    {
        // printf("����Ŀ��λ�ã�%.3f  m\n", dis);
        return false;
    }
}

//bool���� �����Ƿ񵽴�ָ����̬
bool targetpos_reached(double target_posture[], double pos_threshold)
{
    get_compass_angle(&compass_angle);
    double angle = target_posture[2] - compass_angle;
    if (fabs(angle) <= pos_threshold || fabs(angle) >= 2 * PI - pos_threshold)
        return true;
    return false;
}

//��ȡGPS��ֵ
void get_gps_values(double v_gps[])
{
    const double* gps_raw_values = wb_gps_get_values(gps);
    v_gps[0] = gps_raw_values[0];
    v_gps[1] = gps_raw_values[2];
}

//��ѧ����������arctanֵ
double vector2_angle(const double v1[], const double v2[])
{
    return atan2(v2[1], v2[0]) - atan2(v1[1], v1[0]);
}

//�������̽Ƕ�
void get_compass_angle(double* ret_angle)
{
    const double* compass_raw_values = wb_compass_get_values(compass);
    const double v_front[2] = { compass_raw_values[0], compass_raw_values[1] };
    const double v_north[2] = { 1.0, 0.0 };
    *ret_angle = vector2_angle(v_front, v_north) + PI; // angle E(0, 2*PI)
    // printf("��ǰ��̬��%.3f  rad\n", *ret_angle);
}

//��Ʒ��ת��
int nameToIndex(char* name)
{
    for (int i = 0; i < sizeof(GoodsList); i++)
    {
        // printf(" %s : %s \n", name, GoodsList[i]);
        // if (name==GoodsList[i])
        if (strcmp(name, GoodsList[i]) == 0)
            return i;
    }
    return -1;
}

//��Ʒ��ת��
char* indexToName(int index)
{
    return GoodsList[index];
}

//*?                 ���ܺ���        <����>               ?*//