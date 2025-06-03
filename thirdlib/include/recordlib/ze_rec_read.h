/*
¼����ļ���װ˵��
ze_rec_file.h
2015-05-22
*/

#include "ll_headers.h"

#ifndef __ZE_REC_READ_H__
#define __ZE_REC_READ_H__

#define MAX_REC_STREAM_TYPE  4 	//���֧��4������
#define MAX_REC_CHN  16			//ÿ���������֧��16·��Ƶ
//��Ƶͨ������,��u64����,���������ͺϲ���ͨ����(5~6λΪ��������,��4λΪͨ����)������λ������ʾ64λ
#define GET_CHN_MASK(stream_type, chn) 	((u64)0x01 << (((stream_type) & 0x03) << 4 | ((chn) & 0x0F)))

//ͨ������Ƶ��ʽ�ֶζ���
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
#define PLAY_A_TYPE_ADPCM_IMA				16		//ʹ�ú�˼��ʽ
#define PLAY_A_TYPE_G711A_EX				17		//�Ǻ�˼��ʽ
#define PLAY_A_TYPE_G711U_EX				18		//�Ǻ�˼��ʽ
#define PLAY_A_TYPE_AAC_48KBPS				19
/************************************************************
¼���ļ�ͷ��Ϣ����
************************************************************/
#pragma pack(4)
// ͨ����Ϣ
typedef struct
{
	u8	chn;			// ͨ����:��0~64.
	u8	stream_type;	// ��������:0=������,1=������ 
	u8	reserved;		// ����
	u8	v_codec;		// ��Ƶ���뷽ʽ:0=��Ч,1=h264,2=H265
	int	v_fps;			// ֡��
	int	v_bps;			// ����
	u16	v_width;		// ��Ƶͼ���
	u16	v_height;		// ��Ƶͼ���

	u8	a_channels;		// ��Ƶͨ��
	u8	reserved1[2];	// ����
	u8	a_codec;		// ��Ƶ���뷽ʽ����ͨ������Ƶ��ʽ�ֶζ���һ��   old��:0=��Ч,1=g726,2=g711a
	int	a_bps;			// ��Ƶ����
	int	a_sample;		// ��Ƶ������
	int	a_bits;			// ������
}ze_mxm_chn_info_t;
//---sizeof(ze_mxm_chn_info_t)=32

// �ļ�ͷ����
typedef struct
{
	// �ļ���Ϣ
	int	video_norm;			// ��ʽ:0=pal, 1=ntsc
	int	record_type;		// ¼������: 0=normal, 1=alarm (����), -1=��Ч�ļ�(��ɾ��)
	u32	stream_type_mask;	// ������������:��λ��ʾ,0x01=������, 0x02=������
	u64	vchn_mask;			// ��Ƶͨ������:��λ��ʾ,���֧��64λ, GET_CHN_MASK(stream_type, chn)
	u64	achn_mask;			// ��Ƶͨ������:��λ��ʾ,���֧��64λ, GET_CHN_MASK(0, chn)
	u32	fix_file_flag;		// �̶��ļ����:������0, ����û���0, ��ʹ��һ�����ֵ
	
	char serail[16];		// ���к�
	char phone[16];			// �ֻ���
	char vehicle[16];		// ���ƺ�
	u64 alarm_mask;			// ��������(���沿��Э�鶨��ı���) ze_bb_alarm_mask()
	char watermarkPwd[8];	//����ˮӡ����
	u8	reserved[32];		//----ռλ��128
	//
	// ͨ����Ϣ
	int	chn_arrlen;		// ��Чͨ������Ŀ[��ȷ���ж�, ����Ϊ0]
	ze_mxm_chn_info_t	chn_arr[32];

	// ͳ����Ϣ[��Ϊ�Ƿ���Ҫ�޲����ж�]
	u32	open_time;	// ��ʱ��
	u32	close_time;	// �ر�ʱ��
	s64	first_pts;		// ��ʼ֡pts (��λ:us)
	s64	last_pts;		// ����֡pts (��λ:us)
	int	valid_size;		// ��Ч�ļ�����
	int	total_size;		// �ļ��ܳ���

	// ��������
	u8	reserved1[60];	// �����ֶ�
	u8  user_info[800];	//�û��Զ�����Ϣ
}ze_mxm_file_info_t; 	
//---sizeof(ze_mxm_file_info_t)=2048
#define MXM_FILE_INFO_LEN 2048

/************************************************************
¼��֡����
************************************************************/
typedef struct
{
	char tag[4];		// �̶����:MXM_H264I_TAG ~ MXM_ALARM_TAG
	int	fix_file_flag;	// �̶��ļ����
	u8	frame_type;		// ֡����:FRAME_TYPE_H264I ~ FRAME_TYPE_BINARY
	u8	chn;			// ͨ����
	u8	stream_type;	// ��������
	u8	reserved;
	s64	pts;			// ʱ���
	int	frame_len;		// ����
	int	offset;			// �ļ��е�ƫ��
	//char frame_data[0];	// ֡����
}mxm_packet_header;	
//---sizeof(mxm_packet_header)=28
#define MXM_PACKET_HEADER_LEN 28

#define FRAME_TYPE_H264I		0	//I֡
#define FRAME_TYPE_H264P		1	//P֡
#define FRAME_TYPE_AUDIO		2	//��Ƶ֡-G726
#define FRAME_TYPE_BINARY		3	//������֡ : ze_mxm_binary_head_t
#define FRAME_TYPE_ALARM		4	//����֡ : BinEncode_Alarm_s

//��չ��Ƶ֡����
#define FRAME_TYPE_MEDIA_PCM	5	//��Ƶ֡-PCM
#define FRAME_TYPE_G726_16K		10	//��Ƶ֡-G726_16K
#define FRAME_TYPE_G711A		21	//��Ƶ֡-G711A
#define FRAME_TYPE_G711U		22	//��Ƶ֡-G711U

#define MXM_H264I_TAG		"i..i"
#define MXM_H264P_TAG		"p..p"
#define MXM_AUDIO_TAG		"a..a"
#define MXM_BINARY_TAG		"b..b"
#define MXM_ALARM_TAG		"l..l"

/************************************************************
������֡����
************************************************************/
typedef struct
{
	u16 dataType; //��������������:BIN_FRM_GPS ~ BIN_FRM_USERINFO
	u16 dataLen;
	//char bin_data[0];	// ����������
}ze_mxm_binary_head_t;

#define BIN_FRM_GPS			(0x1 << 0) //GPS֡:		BinEncode_GPS_s
#define BIN_FRM_GSENSOR		(0x1 << 1) //GSensor֡:	BinEncode_GSensor_s
#define BIN_FRM_STATUS		(0x1 << 3) //״̬֡:		BinEncode_Status_s
#define BIN_FRM_ALARM		(0x1 << 4) //����֡:		BinEncode_Alarm_s
#define BIN_FRM_DEVINFO		(0x1 << 5) //�豸��Ϣ֡:
#define BIN_FRM_USERINFO	(0x1 << 6) //�û���Ϣ֡:
#define BIN_FRM_ALL			0xffff

//GPS֡���ݽṹ
//#define BinEncode_GPS_s	 GPSData_s
typedef struct
{ 
	char				cGpsStatus; 				//gps�Ƿ���Ч��ʶ:A��Ч,V��Ч��
	char				cSpeedUnit; 				//�ٶȵ�λ
	unsigned short		usSpeed;					//�ٶ�ֵ
	
	char				cLatitudeDegree;			//γ��ֵ�Ķ�
	char				cLatitudeCent; 				//γ��ֵ�ķ�
	char				cLongitudeDegree;			//����ֵ�Ķ�
	char				cLongitudeCent;				//����ֵ�ķ�

	long				lLatitudeSec;				//γ��ֵ����
	long				lLongitudeSec;				//����ֵ����
	unsigned short		usGpsAngle;					//gps�н�
	char				cDirectionLatitude;			//γ�ȵķ���
	char				cDirectionLongitude;		// ���ȵķ��� 

	unsigned short		usGpsAltitude;				//���θ߶�
	unsigned char  		cSatelliteCount;    		//������������
	unsigned char		positionMode; 					//��λģʽ: 0x01-GPS; 0x02-����
	
	//����ʱ����
	double			 	dLatitude;		 			//γ��
	double				dLongitude;			 		//����
}BinEncode_GPS_s;

//���ٶ�֡���ݽṹ
typedef struct
{
	short				x; //x ���ٶ�(��λ0.01g)
	short				y; //y ���ٶ�
	short				z; //z ���ٶ�
}BinEncode_GSensor_s;

//״̬֡���ݽṹ
typedef struct
{
	unsigned char		AccStatus;		//��Կ���źţ�1-Կ���ź���Ч��0-Կ���ź���Ч
	unsigned char		PowerState;		//
	char				reserve[2]; 
	unsigned int		AccVoltage;		// Acc��ѹֵ(��λ0.01V)
	unsigned int		PowerVoltage;	// ��Դ��ѹֵ(��λ0.01V)
	unsigned int		BatteryVoltage;	// ��ص�ѹֵ(��λ0.01V)
	
	unsigned int		IOState;		// ������:��λ��ʾ:DVR_IO_TYPE
	unsigned int		Analogue[2];	// b.2��ģ����
	
	unsigned int 		Pulse;			//����
	int					HddTemperature;  // �����¶�(��λ0.01��)
	int					BoardTemperature;// Ӳ���¶�(��λ0.01��)
}BinEncode_Status_s;

//����֡���ݽṹ
typedef struct
{
	int alarm_type;		//�����ͣ�MDVR_ALARM_TYPE��
	int sub_type;		//�����ͣ�MDVR_ALARM_TYPE��
	int enable;			//0: ����������1: ������ʼ��
	int duration;		//��������ʱ��
	char extdata[128];	//��������
}BinEncode_Alarm_s;

//����֡��ͨ���ź��������Ͷ���
//����ͨ����:����������ʼ��ַ(ZE_ALARM_CHN_E)+��������
//��������:0-��������,1-������ʼ
typedef enum {
	ZE_ALARM_TYPE_UNKNOWN		= 0,  // 0: δ֪(��Ҫ��BinEncode_Alarm_s���֡���ݲ�֪����������)
	ZE_ALARM_TYPE_BASE_IO		= 1,  // 1~31:IO����(DVR_IO_TYPE)  31��  ---����IO����DVR_IO_TYPE�Ǵ�1��ʼ����IO����ͨ���Ų��üӻ���ַ,1�������������
	ZE_ALARM_TYPE_BASE_SPEED	= 32, // 32~36:�ٶȱ���(DVR_SPEED_ALARM_STATE)  8��
	ZE_ALARM_TYPE_BASE_ACCEL	= 40, // 40~45:���ٶȱ���(DVR_GSENSOR_ALARM_STATE) 8��
	ZE_ALARM_TYPE_BASE_TEMP		= 48, // 48~49:�¶ȱ���(����,����)              2��
	ZE_ALARM_TYPE_BASE_POWER	= 50, // 50~52:��ѹ����(����,Ƿѹ,��ѹ)         3��
	ZE_ALARM_TYPE_BASE_GPS_ERR	= 53, // 53~56:GPS�쳣����(��ģ��,ģ�����,����δ��,���߶�·) 4��
	ZE_ALARM_TYPE_BASE_V_MOVED	= 60, // 60~69:�ƶ���ⱨ��(ͨ����:0~9)         10��
	ZE_ALARM_TYPE_BASE_V_LOST	= 70, // 70~79:��Ƶ��ʧ����(ͨ����:0~9)         10��
	ZE_ALARM_TYPE_BASE_V_COVERED= 80, // 80~89:�ڵ�����(ͨ����:0~9)             10��
	ZE_ALARM_TYPE_BASE_DISK_ERR	= 90, // 90~97:���̱���(����ID:0~5)           	8��
	ZE_ALARM_TYPE_BASE_REC_FULL = 98, // 98:¼����       					1��
	ZE_ALARM_TYPE_BASE_AI		= 100,// 100~199:AI����(ALARM_AI_TYPE) 100��
	ZE_ALARM_TYPE_BASE_MAX		= 255,// ����ͨ�������ֻ�ܵ�255
}ZE_ALARM_TYPE_BASE_E;//����������ʼ��ַ

/************************************************************************
version[16]
header_tag[4] + header_len[4] + ze_mxm_file_info_t
idx_tag[4] + idx_len[4] + idx_data[0] + idx_data[1] + .... + idx_data[n] 
zero_tag[4] + zero_len[4] + zero
data_tag[4] + data_len[4] + data
************************************************************************/

#define MXM_VERSION		"mxm_v20160101_00"
#define MXM_VERSION_LEN		16
#define MXM_MIN_TOTAL_SIZE	(10*1024*1024)	// Ԥ�ȴ����ļ�����С�ߴ�
#define MXM_DATA_OFFSET		(5*1024*1024)	// ����ƫ��λ��
#define MXM_TAG_LEN		4
#define MXM_IDX_MAX		((MXM_DATA_OFFSET - MXM_VERSION_LEN - MXM_FILE_INFO_LEN - 4 * 8) / MXM_PACKET_HEADER_LEN)// �����������, �����ļ�ǰ5M�ռ�д�ļ�ͷ������:   (5M - 2K - 32) / 28 = 187171

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
��������
************************************************************************/
//���򿪣�����¼��ͷ��Ϣ��errRepair:�ļ��쳣�޸�(�豸�쳣�ϵ�ʱ���ļ�ͷ����δд����ʱ���ô��޸���ʽ���ļ�,�Զ��޸��ļ�)
int ze_mxm_read_open(ze_mxm_read_t **hand, const char *fullpath, OUT ze_mxm_file_info_t **file_info, int errRepair = 0);

//���ر�
int ze_mxm_read_close(ze_mxm_read_t *hand);

//��֡,������������
int ze_mxm_read_frame(ze_mxm_read_t *hand, OUT int *chn, OUT int *stream_type, OUT int *frame_type, OUT int *frame_len, OUT s64 *frame_pts, OUT char *frame_buf);

//��֡����ʱ�����Գ�������һ֡
int ze_find_next_frame(ze_mxm_read_t *hand, OUT int *chn, OUT int *stream_type, OUT int *frame_type, OUT int *frame_len, OUT s64 *frame_pts, OUT char *frame_buf);

//����ָ��֡,�����϶�
int ze_mxm_read_seek(ze_mxm_read_t *hand, s64 pts);
int ze_mxm_read_seek2time(ze_mxm_read_t *hand, s64 pts);

//����һI֡�򱨾�֡,���ڿ��
int ze_mxm_read_forward_iframe(ze_mxm_read_t *hand, OUT int *chn, OUT int *stream_type, OUT int *frame_type, OUT int *frame_len, OUT s64 *frame_pts, OUT char *frame_buf);

//����һI֡�򱨾�֡,���ڿ���
int ze_mxm_read_backward_iframe(ze_mxm_read_t *hand, OUT int *chn, OUT int *stream_type, OUT int *frame_type, OUT int *frame_len, OUT s64 *frame_pts, OUT char *frame_buf);

//��������֡pts�б�
int ze_mxm_search_alarm_pts(ze_mxm_read_t *hand, int alarm_chn, OUT s64 *ptsList, int ptslistSize);

/*
����ͨ���źͱ�������ת��
��¼���ļ������֡��ͨ���Ŷ���Ϊ����ͨ���ţ��ͱ�������������ת����ϵ
*/
//��������ת����ͨ����
int ze_mxm_alarmType_to_alarmChn(int alarmType, int subType);
//��������ͨ���ź�ת�ɱ��������ͺ�������
int ze_mxm_alarmChn_to_alarmType(int alarmChn, int *alarmType, int *subType);

//���걨������
u64 ze_bb_alarm_mask(u8 alarm_chn);
/************************************************************************
�����ļ�ͷ:
��Ҫ����¼���ļ�������ɾ��
record_type��¼������: 0=normal, 1=alarm (����), -1=��Ч�ļ�(��ɾ��)
user_info:�û��Զ�����Ϣ������������800B
user_info_len���û��Զ�����Ϣ����
************************************************************************/
int ze_mxm_upd_head(const char *fullpath, int record_type, char *user_info, unsigned int user_info_len);

#endif // __ZE_MXM_FILE_H__





