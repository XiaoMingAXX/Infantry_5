#include "drv_can.h"
#include "remote_control.h"
#define RC_CH_VALUE_OFFSET ((uint16_t)1024)
#define ECD_ANGLE_COEF 0.043945f // (360/8192),将编码器值转化为角度制

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
extern RC_ctrl_t rc_ctrl[2];
uint16_t can_cnt_1 = 0;
float Yaw_down;
extern gimbal_t gimbal_Yaw, gimbal_Pitch;
extern chassis_t chassis;
extern shooter_t shooter;

float powerdata[4];
uint16_t pPowerdata[8];

uint16_t setpower = 5500;
int canerror = 0;

void CAN1_Init(void)
{
  CAN_FilterTypeDef can_filter;

  can_filter.FilterBank = 0;                      // filter 0
  can_filter.FilterMode = CAN_FILTERMODE_IDMASK;  // 标识符屏蔽位模式
  can_filter.FilterScale = CAN_FILTERSCALE_32BIT; // 过滤器位宽为单个32位
  can_filter.FilterIdHigh = 0;                    // 标识符寄存器
  can_filter.FilterIdLow = 0;                     // 标识符寄存器
  can_filter.FilterMaskIdHigh = 0;                // 屏蔽寄存器
  can_filter.FilterMaskIdLow = 0;                 // 屏蔽寄存器   set mask 0 to receive all can id
  can_filter.FilterFIFOAssignment = CAN_RX_FIFO0; // assign to fifo0，接收器是FIFO0
  can_filter.FilterActivation = ENABLE;           // enable can filter
  can_filter.SlaveStartFilterBank = 14;

  HAL_CAN_ConfigFilter(&hcan1, &can_filter);                         // init can filter
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING); // 使能can的FIFO0中断
  HAL_CAN_Start(&hcan1);                                             // 启动can1
}

void CAN2_Init(void)
{
  CAN_FilterTypeDef can_filter;

  can_filter.FilterBank = 14;                     // filter 14
  can_filter.FilterMode = CAN_FILTERMODE_IDMASK;  // 标识符屏蔽位模式
  can_filter.FilterScale = CAN_FILTERSCALE_32BIT; // 过滤器位宽为单个32位
  can_filter.FilterIdHigh = 0;                    // 标识符寄存器
  can_filter.FilterIdLow = 0;                     // 标识符寄存器
  can_filter.FilterMaskIdHigh = 0;                // 屏蔽寄存器
  can_filter.FilterMaskIdLow = 0;                 // 屏蔽寄存器   set mask 0 to receive all can id
  can_filter.FilterFIFOAssignment = CAN_RX_FIFO0; // assign to fifo0，接收器是FIFO0
  can_filter.FilterActivation = ENABLE;           // enable can filter
  can_filter.SlaveStartFilterBank = 14;

  HAL_CAN_ConfigFilter(&hcan2, &can_filter); // init can filter
  HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_Start(&hcan2); // 启动can2
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) // 接受中断回调函数
{
  CAN_RxHeaderTypeDef rx_header;

  if (hcan->Instance == CAN1)
  {
    uint8_t rx_data[8];
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data); // receive can1 data

		
		// 接收下DM传来的遥控器数据
    if (rx_header.StdId == 0x33) 
    {
       rc_ctrl[TEMP].rc.rocker_r_ = ((rx_data[0] | (rx_data[1] << 8)) & 0x07ff);                                       //!< Channel 0
       rc_ctrl[TEMP].rc.rocker_r1 = ((((rx_data[1] >> 3) & 0xff) | (rx_data[2] << 5)) & 0x07ff);                       //!< Channel 1
       rc_ctrl[TEMP].rc.rocker_l_ = ((((rx_data[2] >> 6) & 0xff) | (rx_data[3] << 2) | (rx_data[4] << 10)) & 0x07ff); //!< Channel 2
       rc_ctrl[TEMP].rc.rocker_l1 = ((((rx_data[4] >> 1) & 0xff) | (rx_data[5] << 7)) & 0x07ff);                       //!< Channel 3
       rc_ctrl[TEMP].rc.switch_right = ((rx_data[5] >> 4) & 0x0003);     //!< Switch right
       rc_ctrl[TEMP].rc.switch_left = ((rx_data[5] >> 4) & 0x000C) >> 2; //!< Switch left
			// 鼠标解析
       rc_ctrl[TEMP].mouse.x = (rx_data[6] | (rx_data[7] << 8)); //!< Mouse X axis
			
      rc_ctrl[TEMP].rc.rocker_r_ -= RC_CH_VALUE_OFFSET;
      rc_ctrl[TEMP].rc.rocker_r1 -= RC_CH_VALUE_OFFSET;
      rc_ctrl[TEMP].rc.rocker_l_ -= RC_CH_VALUE_OFFSET;
      rc_ctrl[TEMP].rc.rocker_l1 -= RC_CH_VALUE_OFFSET;
    }
    // 接收下DM传来的遥控器数据
    if (rx_header.StdId == 0x34) 
    {
      rc_ctrl[TEMP].mouse.y= rx_data[0] | (rx_data[1] << 8); //!< Mouse Y axis
      rc_ctrl[TEMP].mouse.z= rx_data[2] | (rx_data[3] << 8); //!< Mouse Z axis
      rc_ctrl[TEMP].mouse.press_l= rx_data[4];               //!< Mouse Left Is Press ?
      rc_ctrl[TEMP].mouse.press_r = rx_data[5];               //!< Mouse Right Is Press ?
      rc_ctrl[TEMP].key[0].v = rx_data[6] | (rx_data[7] << 8);   //!< KeyBoard value

    }
			
    // 云台YAW电机信息接收
    if (rx_header.StdId == 0x209) // 判断标识符，标识符为0x204+ID
    {
      gimbal_Yaw.motor_info.rotor_angle = ((rx_data[0] << 8) | rx_data[1]);
      gimbal_Yaw.motor_info.rotor_speed = ((rx_data[2] << 8) | rx_data[3]);
      gimbal_Yaw.motor_info.torque_current = ((rx_data[4] << 8) | rx_data[5]);
      gimbal_Yaw.motor_info.temp = rx_data[6];
    }
		
//		    // 云台PITCH电机信息接收
    if (rx_header.StdId == 0x205) // 判断标识符，标识符为0x204+ID
    {
      gimbal_Pitch.motor_info.rotor_angle = ((rx_data[0] << 8) | rx_data[1]);
      gimbal_Pitch.motor_info.rotor_speed = ((rx_data[2] << 8) | rx_data[3]);
      gimbal_Pitch.motor_info.torque_current = ((rx_data[4] << 8) | rx_data[5]);
      gimbal_Pitch.motor_info.temp = rx_data[6];
    }
  }
	
  if (hcan->Instance == CAN2)
  {
    uint8_t rx_data[8];
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data); // receive can2 data

    // 发射机构电机信息接收
    if ((rx_header.StdId >= 0x205)     // 205-208
        && (rx_header.StdId <= 0x208)) // 判断标识符，标识符为0x200+ID
    {
      uint8_t index = rx_header.StdId - 0x205; // get motor index by can_id
      shooter.motor_info[index].last_ecd = shooter.motor_info[index].rotor_angle;

      shooter.motor_info[index].rotor_angle = ((rx_data[0] << 8) | rx_data[1]);
      shooter.motor_info[index].angle_single_round = ECD_ANGLE_COEF * (float)shooter.motor_info[index].rotor_angle;

      shooter.motor_info[index].rotor_speed = ((rx_data[2] << 8) | rx_data[3]);
      shooter.motor_info[index].torque_current = ((rx_data[4] << 8) | rx_data[5]);
      shooter.motor_info[index].temp = rx_data[6];

      if (shooter.motor_info[index].rotor_angle - shooter.motor_info[index].last_ecd > 4096)
        shooter.motor_info[index].total_round--;
      else if (shooter.motor_info[index].rotor_angle - shooter.motor_info[index].last_ecd < -4096)
        shooter.motor_info[index].total_round++;
      shooter.motor_info[index].total_angle = shooter.motor_info[index].total_round * 360 + shooter.motor_info[index].angle_single_round;

      if (index == 0)
      {
        can_cnt_1++;
      }
    }

		//超级电容信息接收
    if (rx_header.StdId == 0x211)
    {

      extern float powerdata[4];
      uint16_t *pPowerdata = (uint16_t *)rx_data;

      powerdata[0] = (float)pPowerdata[0] / 100.f; // 输入电压
      powerdata[1] = (float)pPowerdata[1] / 100.f; // 电容电压
      powerdata[2] = (float)pPowerdata[2] / 100.f; // 输入电流
      powerdata[3] = (float)pPowerdata[3] / 100.f; // P
    }
  }
}

// 3508电机控制-CAN1
void set_motor_current_chassis(uint8_t id_range, int16_t v1, int16_t v2, int16_t v3, int16_t v4)
{
  CAN_TxHeaderTypeDef tx_header;
  uint8_t tx_data[8];

  tx_header.StdId = (id_range == 0) ? (0x200) : (0x1ff); // 如果id_range==0则等于0x200,id_range==1则等于0x1ff（ID号）
  tx_header.IDE = CAN_ID_STD;                            // 标准帧
  tx_header.RTR = CAN_RTR_DATA;                          // 数据帧

  tx_header.DLC = 8; // 发送数据长度（字节）

  tx_data[0] = (v1 >> 8) & 0xff; // 先发高八位
  tx_data[1] = (v1) & 0xff;
  tx_data[2] = (v2 >> 8) & 0xff;
  tx_data[3] = (v2) & 0xff;
  tx_data[4] = (v3 >> 8) & 0xff;
  tx_data[5] = (v3) & 0xff;
  tx_data[6] = (v4 >> 8) & 0xff;
  tx_data[7] = (v4) & 0xff;
  HAL_CAN_AddTxMessage(&hcan1, &tx_header, tx_data, (uint32_t *)CAN_TX_MAILBOX0);
}

// 6020电机控制-CAN1
void set_motor_current_gimbal(uint8_t id_range, int16_t v1, int16_t v2, int16_t v3, int16_t v4)
{
  CAN_TxHeaderTypeDef tx_header;
  uint8_t tx_data[8];

  tx_header.StdId = (id_range == 0) ? (0x1ff) : (0x2ff); // 如果id_range==0则等于0x1ff,id_range==1则等于0x2ff（ID号）
  tx_header.IDE = CAN_ID_STD;                            // 标准帧
  tx_header.RTR = CAN_RTR_DATA;                          // 数据帧

  tx_header.DLC = 8; // 发送数据长度（字节）

  tx_data[0] = (v1 >> 8) & 0xff; // 先发高八位
  tx_data[1] = (v1) & 0xff;
  tx_data[2] = (v2 >> 8) & 0xff;
  tx_data[3] = (v2) & 0xff;
  tx_data[4] = (v3 >> 8) & 0xff;
  tx_data[5] = (v3) & 0xff;
  tx_data[6] = (v4 >> 8) & 0xff;
  tx_data[7] = (v4) & 0xff;
  HAL_CAN_AddTxMessage(&hcan1, &tx_header, tx_data, (uint32_t *)CAN_TX_MAILBOX0);
}

// 6020电机控制-CAN2
void set_motor_current_gimbal2(uint8_t id_range, int16_t v1, int16_t v2, int16_t v3, int16_t v4)
{
  CAN_TxHeaderTypeDef tx_header;
  uint8_t tx_data[8];

  tx_header.StdId = (id_range == 0) ? (0x1ff) : (0x2ff); // 如果id_range==0则等于0x1ff,id_range==1则等于0x2ff（ID号）
  tx_header.IDE = CAN_ID_STD;                            // 标准帧
  tx_header.RTR = CAN_RTR_DATA;                          // 数据帧

  tx_header.DLC = 8; // 发送数据长度（字节）

  tx_data[0] = (v1 >> 8) & 0xff; // 先发高八位
  tx_data[1] = (v1) & 0xff;
  tx_data[2] = (v2 >> 8) & 0xff;
  tx_data[3] = (v2) & 0xff;
  tx_data[4] = (v3 >> 8) & 0xff;
  tx_data[5] = (v3) & 0xff;
  tx_data[6] = (v4 >> 8) & 0xff;
  tx_data[7] = (v4) & 0xff;
  HAL_CAN_AddTxMessage(&hcan2, &tx_header, tx_data, (uint32_t *)CAN_TX_MAILBOX0);
}

// 3508电机控制-CAN2
void set_motor_current_shoot(uint8_t id_range, int16_t v1, int16_t v2, int16_t v3, int16_t v4)
{
  CAN_TxHeaderTypeDef tx_header;
  uint8_t tx_data[8];

  tx_header.StdId = (id_range == 0) ? (0x200) : (0x1ff); // 如果id_range==0则等于0x200,id_range==1则等于0x1ff（ID号）
  tx_header.IDE = CAN_ID_STD;                            // 标准帧
  tx_header.RTR = CAN_RTR_DATA;                          // 数据帧

  tx_header.DLC = 8; // 发送数据长度（字节）

  tx_data[0] = (v1 >> 8) & 0xff; // 先发高八位
  tx_data[1] = (v1) & 0xff;
  tx_data[2] = (v2 >> 8) & 0xff;
  tx_data[3] = (v2) & 0xff;
  tx_data[4] = (v3 >> 8) & 0xff;
  tx_data[5] = (v3) & 0xff;
  tx_data[6] = (v4 >> 8) & 0xff;
  tx_data[7] = (v4) & 0xff;
  HAL_CAN_AddTxMessage(&hcan2, &tx_header, tx_data, (uint32_t *)CAN_TX_MAILBOX0);
}

// 试运行代码，一步解决所有电机控制
void set_curruent(uint32_t motor_range, CAN_HandleTypeDef can_id, int16_t v1, int16_t v2, int16_t v3, int16_t v4)
{
  CAN_TxHeaderTypeDef tx_header;
  uint8_t tx_data[8];

  tx_header.StdId = motor_range; // 控制电机的ID号
  tx_header.IDE = CAN_ID_STD;    // 标准帧
  tx_header.RTR = CAN_RTR_DATA;  // 数据帧

  tx_header.DLC = 8; // 发送数据长度（字节）

  tx_data[0] = (v1 >> 8) & 0xff; // 先发高八位
  tx_data[1] = (v1) & 0xff;
  tx_data[2] = (v2 >> 8) & 0xff;
  tx_data[3] = (v2) & 0xff;
  tx_data[4] = (v3 >> 8) & 0xff;
  tx_data[5] = (v3) & 0xff;
  tx_data[6] = (v4 >> 8) & 0xff;
  tx_data[7] = (v4) & 0xff;
  HAL_CAN_AddTxMessage(&can_id, &tx_header, tx_data, (uint32_t *)CAN_TX_MAILBOX0);
}

// 向C板发送数据
void can_remote(uint8_t sbus_buf[], uint32_t can_send_id, uint32_t len) // 调用can来发送遥控器数据
{
  CAN_TxHeaderTypeDef tx_header;

  tx_header.StdId = can_send_id;                       // 如果id_range==0则等于0x1ff,id_range==1则等于0x2ff（ID号）
  tx_header.IDE = CAN_ID_STD;                          // 标准帧
  tx_header.RTR = CAN_RTR_DATA;                        // 数据帧
  tx_header.DLC = len;                                   // 发送数据长度（字节）
  while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan2) == 0) // 等待邮箱空闲
  {
  }
  HAL_CAN_AddTxMessage(&hcan1, &tx_header, sbus_buf, (uint32_t *)CAN_TX_MAILBOX0);
}

//void can_remote(uint8_t sbus_buf[], uint8_t can_send_id) // 调用can来发送遥控器数据
//{
//  CAN_TxHeaderTypeDef tx_header;

//  tx_header.StdId = can_send_id; // 如果id_range==0则等于0x1ff,id_range==1则等于0x2ff（ID号）
//  tx_header.IDE = CAN_ID_STD;    // 标准帧
//  tx_header.RTR = CAN_RTR_DATA;  // 数据帧
//  tx_header.DLC = 8;             // 发送数据长度（字节）

//   HAL_CAN_AddTxMessage(&hcan1, &tx_header, sbus_buf, (uint32_t *)CAN_TX_MAILBOX0);
//}