#ifndef __GAME_H__
#define __GAME_H__

#include "FreeRTOS.h"
#include "bsp_uart.h"
#include "comp_cmd.h"
#include "task.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/**
 ****************************(C) COPYRIGHT 2026
 *FHU****************************************
 * @file       	rm.h
 * @author			FHU（飞虎战队）
 * @version			V1.0.0
 * @date 				2026/3/3
 * @brief      	26赛季裁判系统数据
 *
 *
 ****************************(C) COPYRIGHT 2026
 *FHU****************************************
 */
#define JUDGE_VERSION JUDGE_2026
// RoboMaster 2026 机甲大师高校系列赛通信协议 V1.1.0（20251217）

#define auto_remote_len 30
// 数据帧起始字节,协议固定为 0xA5
#define JUDGE_System_FH 0XA5

#define Frame_header_LEN 5 // 帧头长度
#define CMD_ID_LEN 2       // 命令码ID长度
#define Frame_tail_LEN 2   // CRC16帧尾效验长度

#define GAME_STATE_ID 0X0001        // 比赛状态
#define GAME_RESULT_ID 0X0002       // 比赛结果
#define GAME_ROBOT_ID 0X0003        // 全体机器人血量
#define EVENT_DATA_ID 0X0101        // 场地事件
#define EXT_RELOAD_ID 0X0102        // 补弹
#define JUDGE_WARN_ID 0X0104        // 裁判警告
#define DART_INFO_ID 0X0105         // 飞镖状况
#define ROBOT_STATUS_ID 0X0201      // 兵种性能
#define POWER_HEAR_ID 0X0202        // 实时功率热量数据 50HZ
#define ROBOT_POS_ID 0X0203         // 机器人位置数据，哨兵10Hz，其余1Hz
#define BUFF_ID 0X0204              // 增益
#define HURT_ID 0X0206              // 伤害状态数据
#define SHOOT_ID 0X0207             // 发射状态
#define SHOOT_ALLOW_ID 0X0208       // 允许发弹量	10HZ
#define RFID_STATUS_ID 0X0209       // 兵种RFID状态  3HZ
#define DART_CLIENT_CMD_ID 0X020A   // 飞镖选手端指令数据
#define GROUND_ROBOT_POS_ID 0X020B  // 地面机器人位置数据
#define RADAR_MARK_DATA_ID 0X020C   // 雷达标记进度数据
#define SENTRY_INFO_ID 0X020D       // 哨兵自主决策相关信息	1HZ
#define RADAR_INFO_ID 0X020E        // 雷达自主决策信息同步
#define ROBOT_INTERACTION_ID 0X0301 // 机器人交互数据	10HZ
#define CUSTOM_CONTROLLER_ID 0x0302 // 自定义控制器		30HZ
#define map_find 0X0303
#define remote_control_ID 0X0304 //

// 裁判系统数据帧偏移
typedef enum {
  RM_Frame_Header = 0,
  RM_CMD_ID = 5,
  RM_DATA = 7,
} JUDGE_Frame_Offset;

// 5字节帧头,偏移位置
typedef enum {
  SOF = 0,         // 起始位
  DATA_LENGTH = 1, // 帧内数据长度,根据这个来获取数据长度
  SEQ = 3,         // 包序号
  CRC8 = 4         // CRC8
} FrameHeaderOffset;
/* 跨编译器 packed 适配 */
#if defined(__GNUC__)
#define RM_PACKED __attribute__((packed))
#else
#define RM_PACKED
#pragma pack(push, 1)
#endif

struct frame_header_t {
  uint8_t sof;          // 数据帧起始字节,固定值为 0xA5
  uint16_t data_length; // 数据帧中 data 的长度
  uint8_t seq;          // 包序号
  uint8_t crc;          // 帧头 CRC8 校验
} RM_PACKED;

// 0x0001 比赛状态数据, 固定以 1Hz 频率发送, 服务器->全体机器人
struct game_status_t {
  uint8_t game_type : 4;      // 比赛类型: 1-超对, 2-单项, 3-人工智能挑战赛,
                              // 4-联盟赛3v3, 5-联盟赛1v1
  uint8_t game_progress : 4;  // 比赛阶段: 0-未开始, 1-准备阶段, 2-十五秒自检,
                              // 3-五秒倒计时, 4-比赛中, 5-结算中
  uint16_t stage_remain_time; // 当前阶段剩余时间 (秒)
  uint64_t
      SyncTimeStamp; // UNIX 时间, 当机器人正确连接到裁判系统的 NTP 服务器后生效
} RM_PACKED;

// 0x0002 比赛结果数据, 比赛结束触发发送, 服务器->全体机器人
struct game_result_t {
  uint8_t winner; // 0: 平局, 1: 红方获胜, 2: 蓝方获胜
} RM_PACKED;

// 0x0003 机器人血量数据, 固定以 3Hz 频率发送, 服务器->全体机器人
struct game_robot_hp_t {
  uint16_t ally_1_robot_hp; // 己方 1 号英雄机器人血量,
                            // 若该机器人未上场或者被罚下, 则血量为 0, 下文同理
  uint16_t ally_2_robot_hp; // 己方 2 号工程机器人血量
  uint16_t ally_3_robot_hp; // 己方 3 号步兵机器人血量
  uint16_t ally_4_robot_hp; // 己方 4 号步兵机器人血量
  uint16_t reserved;
  uint16_t ally_7_robot_hp; // 己方 7 号哨兵机器人血量
  uint16_t ally_outpost_hp; // 己方前哨站血量
  uint16_t ally_base_hp;    // 己方基地血量
} RM_PACKED;

// 0x0101 场地事件数据, 固定以 1Hz 频率发送, 服务器->己方全体机器人
struct event_data_t {
  uint32_t event_data; // 各位数据表示见文档
} RM_PACKED;
// 0：未占领/未激活
// 1：已占领/已激活
// bit 0-2：
// bit 0：己方与资源区区不重叠的补给区占领状态，1 为已占领
// bit 1：己方与资源区重叠的补给区占领状态，1 为已占领
// bit 2：己方补给区的占领状态，1 为已占领（仅 RMUL 适用）
// bit 3-6：己方能量机关状态
// bit 3-4：己方小能量机关的激活状态，0为未激活，1 为已激活，
// 2为正在激活
// bit 5-6：己方大能量机关的激活状态，0为未激活，1 为已激活，
// 2为正在激活
// bit 7-8：己方中央高地的占领状态，1 为被己方占领，2 为被对方占领
// bit 9-10：己方梯形高地的占领状态，1 为已占领
// bit 11-19：对方飞镖最后一次击中己方前哨站或基地的时间（0-420，开
// 局默认为 0）
// bit 20-22：对方飞镖最后一次击中己方前哨站或基地的具体目标，开局
// 默认为 0，1 为击中前哨站，2 为击中基地固定目标，3 为击中基地随
// 机固定目标，4 为击中基地随机移动目标，5为击中基地末端移动目标
// bit 23-24：中心增益点的占领状态，0 为未被占领，1 为被己方占领，
// 2 为被对方占领，3 为被双方占领。（仅 RMUL 适用）
// bit 25-26：己方堡垒增益点的占领状态，0 为未被占领，1 为被己方占
// 领，2 为被对方占领，3 为被双方占领
// bit 27-28：己方前哨站增益点的占领状态，0 为未被占领，1 为被己方
// 占领，2 为被对方占领
// bit 29：己方基地增益点的占领状态，1 为已占领
// bit 30-31：保留位

// 0x0104 裁判警告数据, 己方判罚/判负时触发发送, 其余时间以 1Hz 频率发送,
// 服务器->被判罚方全体机器人
struct referee_warning_t {
  uint8_t level; // 己方最后一次受到判罚的等级 1：双方黄牌    2：黄牌 3：红牌
                 // 4：判负

  uint8_t offending_robot_id; // 己方最后一次受到判罚的违规机器人 ID, 红 1 为 1,
                              // 蓝 1 为 101
  uint8_t count; // 己方最后一次受到判罚的违规机器人对应判罚等级的违规次数
} RM_PACKED;

// 0x0105 飞镖发射相关数据, 固定以 1Hz频率发送, 服务器->己方全体机器人
struct dart_info_t {
  uint8_t dart_remaining_time; // 己方飞镖发射剩余时间 (秒)
  uint16_t dart_info;          // 飞镖状态信息, 各位数据表示见文档
} RM_PACKED;

// 0x0201 机器人性能体系数据, 固定以 10Hz 频率发送, 主控模块->对应机器人
struct robot_status_t {
  uint8_t robot_id;                           // 本机器人 ID
  uint8_t robot_level;                        // 机器人等级
  uint16_t current_hp;                        // 机器人当前血量
  uint16_t maximum_hp;                        // 机器人血量上限
  uint16_t shooter_barrel_cooling_value;      // 机器人射击热量每秒冷却值
  uint16_t shooter_barrel_heat_limit;         // 机器人射击热量上限
  uint16_t chassis_power_limit;               // 机器人底盘功率上限
  uint8_t power_management_gimbal_output : 1; // 电源管理模块 gimbal 口输出情况
  uint8_t
      power_management_chassis_output : 1; // 电源管理模块 chassis 口输出情况
  uint8_t
      power_management_shooter_output : 1; // 电源管理模块 shooter 口输出情况
} RM_PACKED;

// 0x0202 实时底盘缓冲能量和射击热量数据, 固定以 10Hz 频率发送,
// 主控模块->对应机器人
struct power_heat_data_t {
  uint16_t reserved_1;
  uint16_t reserved_2;
  float reserved_3;
  uint16_t buffer_energy;              // 缓冲能量 (J)
  uint16_t shooter_17mm_1_barrel_heat; // 第 1 个 17mm 发射机构的射击热量
  uint16_t shooter_42mm_barrel_heat;   // 42mm 发射机构的射击热量
} RM_PACKED;

// 0x0203 机器人位置数据, 固定以 1Hz 频率发送, 主控模块->对应机器人
struct robot_pos_t {
  float x;     // 本机器人位置 x 坐标, 单位：m
  float y;     // 本机器人位置 y 坐标, 单位：m
  float angle; // 本机器人测速模块的朝向, 单位：度, 正北为 0 度
} RM_PACKED;

// 0x0204 机器人增益和底盘能量数据, 固定以 3Hz 频率发送, 服务器->对应机器人
struct buff_t {
  uint8_t recovery_buff; // 机器人回血增益（百分比, 值为 10
                         // 表示每秒恢复血量上限的 10%）
  uint16_t cooling_buff; // 机器人射击热量冷却增益具体值（直接值, 值为 x
                         // 表示热量冷却增加 x/s）
  uint8_t defence_buff;  // 机器人防御增益（百分比, 值为 50 表示 50%防御增益）
  uint8_t vulnerability_buff; // 机器人负防御增益（百分比, 值为 30
                              // 表示-30%防御增益）
  uint16_t attack_buff; // 机器人攻击增益（百分比, 值为 50 表示 50%攻击增益）
  uint8_t remaining_energy; // 机器人剩余能量值反馈, 具体表示见文档
} RM_PACKED;

// 0x0206 伤害状态数据, 伤害发生后发送, 主控模块->对应机器人
struct hurt_data_t {
  uint8_t armor_id; // 当扣血原因为装甲模块被弹丸攻击、受撞击或离线时, 该 4 bit
                    // 组成的数值为装甲模块或测速模块的 ID
                    // 编号；当其他原因导致扣血时, 该数值为 0
  uint8_t hp_deduction_reason; // 血量变化类型: 0-装甲模块被弹丸攻击导致扣血,
                               // 1-装甲模块或超级电容管理模块离线导致扣血,
                               // 5-装甲模块受到撞击导致扣血
} RM_PACKED;

// 0x0207 实时射击数据, 弹丸发射后发送, 主控模块->对应机器人
struct shoot_data_t {
  uint8_t bullet_type;         // 弹丸类型
  uint8_t shooter_number;      // 发射机构 ID
  uint8_t launching_frequency; // 弹丸射速（单位：Hz）
  float initial_speed;         // 弹丸初速度（单位：m/s）
} RM_PACKED;

// 0x0208 允许发弹量, 固定以 10Hz 频率发送,
// 服务器->己方英雄、步兵、哨兵、空中机器人
struct projectile_allowance_t {
  uint16_t projectile_allowance_17mm; // 机器人自身拥有的 17mm 弹丸允许发弹量
  uint16_t projectile_allowance_42mm; // 42mm 弹丸允许发弹量
  uint16_t remaining_gold_coin;       // 剩余金币数量
  uint16_t
      projectile_allowance_fortress; // 堡垒增益点提供的储备 17mm 弹丸允许发弹量
                                     // (该值与机器人是否实际占领堡垒无关)
} RM_PACKED;

// 0x0209 机器人 RFID 模块状态, 固定以 3Hz 频率发送, 服务器->己方装有 RFID
// 模块的机器人
struct rfid_status_t {
  uint32_t rfid_status;
  uint8_t rfid_status_2;
} RM_PACKED;

// 0x020a 飞镖选手端指令数据, 固定以 3Hz 频率发送, 服务器->己方飞镖机器人
struct dart_client_cmd_t {
  uint8_t dart_launch_opening_status; // 当前飞镖发射站的状态
  uint8_t reserved;
  uint16_t target_change_time; // 切换击打目标时的比赛剩余时间, 单位：秒,
                               // 无/未切换动作, 默认为 0
  uint16_t
      latest_launch_cmd_time; // 最后一次操作手确定发射指令时的比赛剩余时间,
                              // 单位：秒, 初始值为 0
} RM_PACKED;

// 0x020b 地面机器人位置数据, 固定以 1Hz 频率发送, 服务器->己方哨兵机器人
struct ground_robot_position_t {
  float hero_x;       // 己方英雄机器人位置 x 轴坐标, 单位：m
  float hero_y;       // 己方英雄机器人位置 y 轴坐标, 单位：m
  float engineer_x;   // 己方工程机器人位置 x 轴坐标, 单位：m
  float engineer_y;   // 己方工程机器人位置 y 轴坐标, 单位：m
  float standard_3_x; // 己方 3 号步兵机器人位置 x 轴坐标, 单位：m
  float standard_3_y; // 己方 3 号步兵机器人位置 y 轴坐标, 单位：m
  float standard_4_x; // 己方 4 号步兵机器人位置 x 轴坐标, 单位：m
  float standard_4_y; // 己方 4 号步兵机器人位置 y 轴坐标, 单位：m
  float reserved_1;
  float reserved_2;
} RM_PACKED;

// 0x020c 雷达标记进度数据, 固定以 1Hz 频率发送, 服务器->己方雷达机器人
struct radar_mark_data_t {
  uint16_t mark_progress; // 含义见文档
} RM_PACKED;

// 0x020d 哨兵自主决策信息同步, 固定以 1Hz 频率发送, 服务器->己方哨兵机器人
struct sentry_info_t {
  uint32_t sentry_info;
  uint16_t sentry_info_2;
} RM_PACKED;

// 0x020e 雷达自主决策信息同步, 固定以 1Hz 频率发送, 服务器->己方雷达机器人
struct radar_info_t {
  uint8_t radar_info;
} RM_PACKED;

// 0x0301   机器人交互数据，发送方触发发送，频率上限为30Hz

struct robot_interaction_data_t {
  uint16_t data_cmd_id;
  uint16_t sender_id;
  uint16_t receiver_id;
  uint8_t user_data[112];
} RM_PACKED;

// 图传链路
// 0x0302  自定义控制器与机器人交互数据，发送方触发发送，频率上限为30Hz
// 自定义控制器→选手端图传连接的机器人
struct custom_robot_data_t {
  uint8_t data[auto_remote_len];
} RM_PACKED;

// 0x0303 选手端小地图交互数据，选手端触发发送
// 选手端点击→服务器→发送方选择的己方机器人

struct map_command_t {
  float
      target_position_x; // 目标位置x轴坐标，单位m 当发送目标机器人ID时，该值为0
  float
      target_position_y; // 目标位置y轴坐标，单位m 当发送目标机器人ID时，该值为0
  uint8_t cmd_keyboard;  // 云台手按下的键盘按键通用键值   无按键按下，则为0
  uint8_t target_robot_id; // 对方机器人ID   当发送坐标数据时，该值为0
  uint16_t cmd_source;     // 信息来源ID    自己找
} RM_PACKED;

// 图传链路
// 0x0304  键鼠遥控数据，固定30Hz频率发送  选手端→选手端图传连接的机器人

struct remote_control_t {
  int16_t mouse_x : 2;          // 鼠标x轴移动速度，负值标识向左移动
  int16_t mouse_y : 2;          // 鼠标y轴移动速度，负值标识向上移动
  int16_t mouse_z : 2;          // 鼠标滚轮移动速度，负值标识向后滚动
  int8_t left_button_down : 1;  // 鼠标左键是否按下：0为未按下；1为按下
  int8_t right_button_down : 1; // 鼠标右键是否按下：0为未按下；1为按下
  uint16_t W : 1;               // W键是否按下：0为未按下；1为按下
  uint16_t S : 1;               // S键是否 按下：0为未按下；1为按下
  uint16_t A : 1;               // A键是否按下：0为未按下；1为按下
  uint16_t D : 1;               // D键是否按下：0为未按下；1为按下
  uint16_t shift : 1;           // Shift键是否按下：0为未按下；1为按下
  uint16_t ctrl : 1;            // Ctrl键是否按下：0为未按下；1为按下
  uint16_t Q : 1;               // Q键是否按下：0为未按下；1为按下
  uint16_t E : 1;               // E键是否按下：0为未按下；1为按下
  uint16_t R : 1;               // R键是否按下：0为未按下；1为按下
  uint16_t F : 1;               // F键是否按下：0为未按下；1为按下
  uint16_t G : 1;               // G键是否按下：0为未按下；1为按下
  uint16_t Z : 1;               // Z键是 否按下：0为未按下；1为按下
  uint16_t X : 1;               // X键是否按下：0为未按下；1为按下
  uint16_t C : 1;               // C键是否按下：0为未按下；1为按下
  uint16_t V : 1;               // V键是否按下：0为未按下；1为按下
  uint16_t B : 1;               // B键是否按下：0为未按下；1为按下
  uint16_t reserved : 2;
} RM_PACKED;

typedef struct RM_PACKED {
  // 0x0001 比赛状态数据, 固定以 1Hz 频率发送, 服务器->全体机器人
  struct game_status_t game_status;
  // 0x0002 比赛结果数据, 比赛结束触发发送, 服务器->全体机器人
  struct game_result_t game_result;
  // 0x0003 机器人血量数据, 固定以 3Hz 频率发送, 服务器->全体机器人
  struct game_robot_hp_t game_robot_hp;
  // 0x0101 场地事件数据, 固定以 1Hz 频率发送, 服务器->己方全体机器人
  struct event_data_t event_data;
  // 0x0104 裁判警告数据, 己方判罚/判负时触发发送, 其余时间以 1Hz 频率发送,
  // 服务器->被判罚方全体机器人
  struct referee_warning_t referee_warning;
  // 0x0105 飞镖发射相关数据, 固定以 1Hz频率发送, 服务器->己方全体机器人
  struct dart_info_t dart_info;
  // 0x0201 机器人性能体系数据, 固定以 10Hz 频率发送, 主控模块->对应机器人
  struct robot_status_t robot_status;
  // 0x0202 实时底盘缓冲能量和射击热量数据, 固定以 10Hz 频率发送,
  // 主控模块->对应机器人
  struct power_heat_data_t power_heat_data;
  // 0x0203 机器人位置数据, 固定以 1Hz 频率发送, 主控模块->对应机器人
  struct robot_pos_t robot_pos;
  // 0x0204 机器人增益和底盘能量数据, 固定以 3Hz 频率发送, 服务器->对应机器人
  struct buff_t buff;
  // 0x0206 伤害状态数据, 伤害发生后发送, 主控模块->对应机器人
  struct hurt_data_t hurt_data;
  // 0x0207 实时射击数据, 弹丸发射后发送, 主控模块->对应机器人
  struct shoot_data_t shoot_data;
  // 0x0208 允许发弹量, 固定以 10Hz 频率发送,
  // 服务器->己方英雄、步兵、哨兵、空中机器人
  struct projectile_allowance_t projectile_allowance;
  // 0x0209 机器人 RFID 模块状态, 固定以 3Hz 频率发送, 服务器->己方装有 RFID
  // 模块的机器人
  struct rfid_status_t rfid_status;
  // 0x020a 飞镖选手端指令数据, 固定以 3Hz 频率发送, 服务器->己方飞镖机器人
  struct dart_client_cmd_t dart_client_cmd;
  // 0x020b 地面机器人位置数据, 固定以 1Hz 频率发送, 服务器->己方哨兵机器人
  struct ground_robot_position_t ground_robot_position;
  // 0x020c 雷达标记进度数据, 固定以 1Hz 频率发送, 服务器->己方雷达机器人
  struct radar_mark_data_t radar_mark_data;
  // 0x020d 哨兵自主决策信息同步, 固定以 1Hz 频率发送, 服务器->己方哨兵机器人
  struct sentry_info_t sentry_info;
  // 0x020e 雷达自主决策信息同步, 固定以 1Hz 频率发送, 服务器->己方雷达机器人
  struct radar_info_t radar_info;
} data_t;

/**
 * @brief 通信数据类型定义
 * 
 * 该结构体定义了机器人与裁判系统之间通信所需的所有数据类型，
 * 包括裁判系统数据、机器人交互数据、自定义机器人数据、地图命令和遥控器数据。
 * 使用RM_PACKED进行内存对齐优化，确保数据传输的一致性。
 */
typedef struct RM_PACKED {
  data_t judge_data;                              ///< 裁判系统数据，包含游戏状态、机器人状态、伤害信息等
  struct robot_interaction_data_t robot_interaction_data;  ///< 机器人间交互数据，用于机器人之间的通信
  struct custom_robot_data_t custom_robot_data;            ///< 自定义机器人数据，包含机器人特有的控制和状态信息
  struct map_command_t map_command;                        ///< 地图命令数据，用于地图相关的指令和信息
  struct remote_control_t remote_control_data;             ///< 遥控器数据，包含来自遥控器的控制指令
} CommuniCateTypeDef;

#ifndef GAME_RX_DMA_BUF_SIZE
#define GAME_RX_DMA_BUF_SIZE 128U
#endif

#ifndef GAME_RX_FIFO_LEN
#define GAME_RX_FIFO_LEN 512U
#endif

#ifndef GAME_MAX_PAYLOAD_LEN
#define GAME_MAX_PAYLOAD_LEN 128U
#endif

#define GAME_SIGNAL_RAW_READY (1u << 4)
#define GAME_MIN_FRAME_LEN (Frame_header_LEN + CMD_ID_LEN + Frame_tail_LEN)
#define GAME_MAX_FRAME_LEN                                                     \
  (Frame_header_LEN + CMD_ID_LEN + GAME_MAX_PAYLOAD_LEN + Frame_tail_LEN)
#define GAME_STASH_CAPACITY (GAME_MAX_FRAME_LEN + GAME_RX_DMA_BUF_SIZE)

//! 裁判系统接收对象。
//! RX 使用 UART 循环 DMA 接收字节流，ISR 写入 FIFO，任务上下文完成粘包/半包解析。
typedef struct {
  TaskHandle_t thread_alert;       //! 接收事件通知的 FreeRTOS 任务句柄
  CommuniCateTypeDef *target;      //! 解析结果写入目标，通常为 custom_robot_data
  STM32UART_t uart_receive_;       //! UART DMA 接收通道
  err_t init_error_;               //! BSP 初始化结果，Start 前保留
  uint8_t rx_fifo[GAME_RX_FIFO_LEN]; //! ISR 到任务的环形 FIFO
  volatile uint16_t rx_head;       //! FIFO 写指针，由 ISR 推进
  volatile uint16_t rx_tail;       //! FIFO 读指针，由任务推进
  volatile bool rx_fifo_overflowed; //! FIFO 溢出标志，任务中消费并恢复解析状态
  uint32_t rx_fifo_overflow_count; //! FIFO 溢出计数，便于调试链路负载
  uint8_t stash[GAME_STASH_CAPACITY]; //! 半包/粘包暂存缓冲
  size_t stash_len;                //! 暂存缓冲当前有效长度
} Game_t;

extern CommuniCateTypeDef custom_robot_data;

void remote_control_data_init(void);
void remote_process(uint8_t *data, CommuniCateTypeDef *custom_robot_data);

//! 初始化 game 对象，绑定 UART RX DMA 和解析目标。
err_t Game_Init(Game_t *self, UART_HandleTypeDef *uart_handle,
                CommuniCateTypeDef *target);
//! 启动 UART 循环 DMA 接收。
err_t Game_Start(Game_t *self);
//! 任务周期入口：等待 RX 通知，处理 FIFO 和裁判系统帧解析。
void Game_Update(Game_t *self, uint32_t timeout_ms);

#if !defined(__GNUC__)
#pragma pack(pop)
#endif

#endif
