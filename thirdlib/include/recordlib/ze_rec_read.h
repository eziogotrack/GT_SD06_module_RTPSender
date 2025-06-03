/*
录像库文件封装说明
ze_rec_file.h
2015-05-22
*/

#include "ll_headers.h"

#ifndef __ZE_REC_READ_H__
#define __ZE_REC_READ_H__

#define MAX_REC_STREAM_TYPE  4 	//最大支持4种码流
#define MAX_REC_CHN  16			//每种码流最大支持16路视频
//视频通道掩码,用u64保存,把码流类型合并进通道号(5~6位为码流类型,低4位为通道号)，再移位，最大表示64位
#define GET_CHN_MASK(stream_type, chn) 	((u64)0x01 << (((stream_type) & 0x03) << 4 | ((chn) & 0x0F)))

//通天星音频格式字段定义
#define PLAY_A_TYPE_UNKOWN					0
#define PLAY_A_TYPE_G726_40KBPS				1
#define PLAY_A_TYPE_ADPCM					2
#define PLAY_A_TYPE_G726_MEDIA_40KBPS		3
#define PLAY_A_TYPE_G726_MEDIA_32KBPS		4
#define PLAY_A_TYPE_G726_MEDIA_24KBPS		5
#define PLAY_A_TYPE_G726_MEDIA_16KBPS		6
#define PLAY_A_TYPE_G726_32KBPS				7
#define PLAY_A_TYPE_G726_24KBPS				8
#define PLAY_A_TYPE_G726_16KBPS				9
#define PLAY_A_TYPE_HI_G711A				10
#define PLAY_A_TYPE_HI_G711U				11
#define PLAY_A_TYPE_AAC_8KBPS				12
#define PLAY_A_TYPE_AAC_16KBPS				13
#define PLAY_A_TYPE_AMR						14
#define PLAY_A_TYPE_AAC_24KBPS				15
#define PLAY_A_TYPE_ADPCM_IMA				16		//使用海思格式
#define PLAY_A_TYPE_G711A_EX				17		//非海思格式
#define PLAY_A_TYPE_G711U_EX				18		//非海思格式
#define PLAY_A_TYPE_AAC_48KBPS				19
/************************************************************
录像文件头信息定义
************************************************************/
#pragma pack(4)
// 通道信息
typedef struct
{
	u8	chn;			// 通道号:从0~64.
	u8	stream_type;	// 码流类型:0=主码流,1=子码流 
	u8	reserved;		// 保留
	u8	v_codec;		// 视频编码方式:0=无效,1=h264,2=H265
	int	v_fps;			// 帧率
	int	v_bps;			// 码率
	u16	v_width;		// 视频图像宽
	u16	v_height;		// 视频图像高

	u8	a_channels;		// 音频通道
	u8	reserved1[2];	// 保留
	u8	a_codec;		// 音频编码方式：和通天星音频格式字段定义一致   old：:0=无效,1=g726,2=g711a
	int	a_bps;			// 音频码率
	int	a_sample;		// 音频采样率
	int	a_bits;			// 比特数
}ze_mxm_chn_info_t;
//---sizeof(ze_mxm_chn_info_t)=32

// 文件头定义
typedef struct
{
	// 文件信息
	int	video_norm;			// 制式:0=pal, 1=ntsc
	int	record_type;		// 录像类型: 0=normal, 1=alarm (锁定), -1=无效文件(已删除)
	u32	stream_type_mask;	// 码流类型掩码:按位表示,0x01=主码流, 0x02=子码流
	u64	vchn_mask;			// 视频通道掩码:按位表示,最大支持64位, GET_CHN_MASK(stream_type, chn)
	u64	achn_mask;			// 音频通道掩码:按位表示,最大支持64位, GET_CHN_MASK(0, chn)
	u32	fix_file_flag;		// 固定文件标记:不能填0, 如果用户填0, 将使用一个随机值
	
	char serail[16];		// 序列号
	char phone[16];			// 手机号
	char vehicle[16];		// 车牌号
	u64 alarm_mask;			// 报警掩码(保存部标协议定义的报警) ze_bb_alarm_mask()
	char watermarkPwd[8];	//数字水印密码
	u8	reserved[32];		//----占位至128
	//
	// 通道信息
	int	chn_arrlen;		// 有效通道的数目[正确性判断, 不能为0]
	ze_mxm_chn_info_t	chn_arr[32];

	// 统计信息[作为是否需要修补的判断]
	u32	open_time;	// 打开时间
	u32	close_time;	// 关闭时间
	s64	first_pts;		// 起始帧pts (单位:us)
	s64	last_pts;		// 结束帧pts (单位:us)
	int	valid_size;		// 有效文件长度
	int	total_size;		// 文件总长度

	// 保留扩充
	u8	reserved1[60];	// 保留字段
	u8  user_info[800];	//用户自定义信息
}ze_mxm_file_info_t; 	
//---sizeof(ze_mxm_file_info_t)=2048
#define MXM_FILE_INFO_LEN 2048

/************************************************************
录像帧定义
************************************************************/
typedef struct
{
	char tag[4];		// 固定标记:MXM_H264I_TAG ~ MXM_ALARM_TAG
	int	fix_file_flag;	// 固定文件标记
	u8	frame_type;		// 帧类型:FRAME_TYPE_H264I ~ FRAME_TYPE_BINARY
	u8	chn;			// 通道号
	u8	stream_type;	// 码流类型
	u8	reserved;
	s64	pts;			// 时间戳
	int	frame_len;		// 长度
	int	offset;			// 文件中的偏移
	//char frame_data[0];	// 帧数据
}mxm_packet_header;	
//---sizeof(mxm_packet_header)=28
#define MXM_PACKET_HEADER_LEN 28

#define FRAME_TYPE_H264I		0	//I帧
#define FRAME_TYPE_H264P		1	//P帧
#define FRAME_TYPE_AUDIO		2	//音频帧-G726
#define FRAME_TYPE_BINARY		3	//二进制帧 : ze_mxm_binary_head_t
#define FRAME_TYPE_ALARM		4	//报警帧 : BinEncode_Alarm_s

//扩展音频帧类型
#define FRAME_TYPE_MEDIA_PCM	5	//音频帧-PCM
#define FRAME_TYPE_G726_16K		10	//音频帧-G726_16K
#define FRAME_TYPE_G711A		21	//音频帧-G711A
#define FRAME_TYPE_G711U		22	//音频帧-G711U

#define MXM_H264I_TAG		"i..i"
#define MXM_H264P_TAG		"p..p"
#define MXM_AUDIO_TAG		"a..a"
#define MXM_BINARY_TAG		"b..b"
#define MXM_ALARM_TAG		"l..l"

/************************************************************
二进制帧定义
************************************************************/
typedef struct
{
	u16 dataType; //二进制数据类型:BIN_FRM_GPS ~ BIN_FRM_USERINFO
	u16 dataLen;
	//char bin_data[0];	// 二进制数据
}ze_mxm_binary_head_t;

#define BIN_FRM_GPS			(0x1 << 0) //GPS帧:		BinEncode_GPS_s
#define BIN_FRM_GSENSOR		(0x1 << 1) //GSensor帧:	BinEncode_GSensor_s
#define BIN_FRM_STATUS		(0x1 << 3) //状态帧:		BinEncode_Status_s
#define BIN_FRM_ALARM		(0x1 << 4) //报警帧:		BinEncode_Alarm_s
#define BIN_FRM_DEVINFO		(0x1 << 5) //设备信息帧:
#define BIN_FRM_USERINFO	(0x1 << 6) //用户信息帧:
#define BIN_FRM_ALL			0xffff

//GPS帧数据结构
//#define BinEncode_GPS_s	 GPSData_s
typedef struct
{ 
	char				cGpsStatus; 				//gps是否有效标识:A有效,V无效。
	char				cSpeedUnit; 				//速度单位
	unsigned short		usSpeed;					//速度值
	
	char				cLatitudeDegree;			//纬度值的度
	char				cLatitudeCent; 				//纬度值的分
	char				cLongitudeDegree;			//经度值的度
	char				cLongitudeCent;				//经度值的分

	long				lLatitudeSec;				//纬度值的秒
	long				lLongitudeSec;				//经度值的秒
	unsigned short		usGpsAngle;					//gps夹角
	char				cDirectionLatitude;			//纬度的方向
	char				cDirectionLongitude;		// 经度的方向 

	unsigned short		usGpsAltitude;				//海拔高度
	unsigned char  		cSatelliteCount;    		//可视卫星总数
	unsigned char		positionMode; 					//定位模式: 0x01-GPS; 0x02-北斗
	
	//计算时好用
	double			 	dLatitude;		 			//纬度
	double				dLongitude;			 		//经度
}BinEncode_GPS_s;

//加速度帧数据结构
typedef struct
{
	short				x; //x 加速度(单位0.01g)
	short				y; //y 加速度
	short				z; //z 加速度
}BinEncode_GSensor_s;

//状态帧数据结构
typedef struct
{
	unsigned char		AccStatus;		//车钥匙信号，1-钥匙信号有效，0-钥匙信号无效
	unsigned char		PowerState;		//
	char				reserve[2]; 
	unsigned int		AccVoltage;		// Acc电压值(单位0.01V)
	unsigned int		PowerVoltage;	// 电源电压值(单位0.01V)
	unsigned int		BatteryVoltage;	// 电池电压值(单位0.01V)
	
	unsigned int		IOState;		// 开关量:按位表示:DVR_IO_TYPE
	unsigned int		Analogue[2];	// b.2个模拟量
	
	unsigned int 		Pulse;			//脉冲
	int					HddTemperature;  // 主板温度(单位0.01℃)
	int					BoardTemperature;// 硬盘温度(单位0.01℃)
}BinEncode_Status_s;

//报警帧数据结构
typedef struct
{
	int alarm_type;		//主类型，MDVR_ALARM_TYPE。
	int sub_type;		//子类型，MDVR_ALARM_TYPE。
	int enable;			//0: 报警结束，1: 报警开始。
	int duration;		//报警持续时间
	char extdata[128];	//附加数据
}BinEncode_Alarm_s;

//报警帧的通道号和码流类型定义
//报警通道号:报警类型起始地址(ZE_ALARM_CHN_E)+报警类型
//码流类型:0-报警结束,1-报警开始
typedef enum {
	ZE_ALARM_TYPE_UNKNOWN		= 0,  // 0: 未知(需要按BinEncode_Alarm_s解出帧数据才知道报警类型)
	ZE_ALARM_TYPE_BASE_IO		= 1,  // 1~31:IO报警(DVR_IO_TYPE)  31个  ---（因IO报警DVR_IO_TYPE是从1开始，故IO报警通道号不用加基地址,1代表紧急报警）
	ZE_ALARM_TYPE_BASE_SPEED	= 32, // 32~36:速度报警(DVR_SPEED_ALARM_STATE)  8个
	ZE_ALARM_TYPE_BASE_ACCEL	= 40, // 40~45:加速度报警(DVR_GSENSOR_ALARM_STATE) 8个
	ZE_ALARM_TYPE_BASE_TEMP		= 48, // 48~49:温度报警(低温,高温)              2个
	ZE_ALARM_TYPE_BASE_POWER	= 50, // 50~52:电压报警(掉电,欠压,超压)         3个
	ZE_ALARM_TYPE_BASE_GPS_ERR	= 53, // 53~56:GPS异常报警(无模块,模块故障,天线未接,天线短路) 4个
	ZE_ALARM_TYPE_BASE_V_MOVED	= 60, // 60~69:移动侦测报警(通道号:0~9)         10个
	ZE_ALARM_TYPE_BASE_V_LOST	= 70, // 70~79:视频丢失报警(通道号:0~9)         10个
	ZE_ALARM_TYPE_BASE_V_COVERED= 80, // 80~89:遮挡报警(通道号:0~9)             10个
	ZE_ALARM_TYPE_BASE_DISK_ERR	= 90, // 90~97:磁盘报警(磁盘ID:0~5)           	8个
	ZE_ALARM_TYPE_BASE_REC_FULL = 98, // 98:录像满       					1个
	ZE_ALARM_TYPE_BASE_AI		= 100,// 100~199:AI报警(ALARM_AI_TYPE) 100个
	ZE_ALARM_TYPE_BASE_MAX		= 255,// 报警通道号最大只能到255
}ZE_ALARM_TYPE_BASE_E;//报警类型起始地址

/************************************************************************
version[16]
header_tag[4] + header_len[4] + ze_mxm_file_info_t
idx_tag[4] + idx_len[4] + idx_data[0] + idx_data[1] + .... + idx_data[n] 
zero_tag[4] + zero_len[4] + zero
data_tag[4] + data_len[4] + data
************************************************************************/

#define MXM_VERSION		"mxm_v20160101_00"
#define MXM_VERSION_LEN		16
#define MXM_MIN_TOTAL_SIZE	(10*1024*1024)	// 预先创建文件的最小尺寸
#define MXM_DATA_OFFSET		(5*1024*1024)	// 数据偏移位置
#define MXM_TAG_LEN		4
#define MXM_IDX_MAX		((MXM_DATA_OFFSET - MXM_VERSION_LEN - MXM_FILE_INFO_LEN - 4 * 8) / MXM_PACKET_HEADER_LEN)// 最大索引数量, 分配文件前5M空间写文件头和索引:   (5M - 2K - 32) / 28 = 187171

#define MXM_HEADER_TAG		"head"
#define MXM_IDX_TAG			"idx1"
#define MXM_ZERO_TAG		"zero"
#define MXM_DATA_TAG		"data"

struct ze_mxm_read_t
{
	char	fullpath[512];

	char	version[MXM_VERSION_LEN];
	ze_mxm_file_info_t	file_info;

	int		idx_count;
	mxm_packet_header	idx_arr[MXM_IDX_MAX];

	FILE	*fd;
	int		next_idx_offset;
	int		next_read_offset;

	int		off_header;
	int		len_header;
	int		off_idx1;
	int		len_idx1;
	int		off_data;
	int		len_data;
};

#pragma pack()

/************************************************************************
读出操作
************************************************************************/
//读打开，返回录像头信息，errRepair:文件异常修复(设备异常断电时，文件头可能未写，此时可用带修复方式打开文件,自动修复文件)
int ze_mxm_read_open(ze_mxm_read_t **hand, const char *fullpath, OUT ze_mxm_file_info_t **file_info, int errRepair = 0);

//读关闭
int ze_mxm_read_close(ze_mxm_read_t *hand);

//读帧,用于正常播放
int ze_mxm_read_frame(ze_mxm_read_t *hand, OUT int *chn, OUT int *stream_type, OUT int *frame_type, OUT int *frame_len, OUT s64 *frame_pts, OUT char *frame_buf);

//读帧出错时，可以尝试找下一帧
int ze_find_next_frame(ze_mxm_read_t *hand, OUT int *chn, OUT int *stream_type, OUT int *frame_type, OUT int *frame_len, OUT s64 *frame_pts, OUT char *frame_buf);

//跳到指定帧,用于拖动
int ze_mxm_read_seek(ze_mxm_read_t *hand, s64 pts);
int ze_mxm_read_seek2time(ze_mxm_read_t *hand, s64 pts);

//读下一I帧或报警帧,用于快进
int ze_mxm_read_forward_iframe(ze_mxm_read_t *hand, OUT int *chn, OUT int *stream_type, OUT int *frame_type, OUT int *frame_len, OUT s64 *frame_pts, OUT char *frame_buf);

//读上一I帧或报警帧,用于快退
int ze_mxm_read_backward_iframe(ze_mxm_read_t *hand, OUT int *chn, OUT int *stream_type, OUT int *frame_type, OUT int *frame_len, OUT s64 *frame_pts, OUT char *frame_buf);

//搜索报警帧pts列表
int ze_mxm_search_alarm_pts(ze_mxm_read_t *hand, int alarm_chn, OUT s64 *ptsList, int ptslistSize);

/*
报警通道号和报警类型转换
在录像文件里，报警帧的通道号定义为报警通道号，和报警类型有如下转换关系
*/
//报警类型转报警通道号
int ze_mxm_alarmType_to_alarmChn(int alarmType, int subType);
//读出报警通道号后，转成报警主类型和子类型
int ze_mxm_alarmChn_to_alarmType(int alarmChn, int *alarmType, int *subType);

//部标报警掩码
u64 ze_bb_alarm_mask(u8 alarm_chn);
/************************************************************************
更新文件头:
主要用于录像文件锁定和删除
record_type：录像类型: 0=normal, 1=alarm (锁定), -1=无效文件(已删除)
user_info:用户自定义信息，长度限制在800B
user_info_len：用户自定义信息长度
************************************************************************/
int ze_mxm_upd_head(const char *fullpath, int record_type, char *user_info, unsigned int user_info_len);

#endif // __ZE_MXM_FILE_H__





