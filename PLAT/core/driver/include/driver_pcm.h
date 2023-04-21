#ifndef __CORE_PCM_H__
#define __CORE_PCM_H__
#include "bsp_common.h"

typedef enum
{
	AUDIO_PCM_PLAY_8K16BIT  = 8000,
	AUDIO_PCM_PLAY_16K16BIT = 16000,
}Audio_PCM_SampleRate;

typedef struct
{
    uint32_t len;
    uint8_t *buf;
}PCM_BufStruct;

typedef struct
{
	CBFuncEx_t cb;
	void *user_param;
	uint8_t *tx_data;
	uint32_t total_len;
	uint8_t  IsBusy;
}PCM_ResourceStruct;

/**
 * @brief PCM播报音频初始化配置
 *
 * @param SampleRate : PCM播报音频采样率
 */
int PCM_Start(uint32_t SampleRate);
void PCM_Tx(uint8_t* Data, uint32_t ByteLen, CBFuncEx_t cb, void *param);
void PCM_TxStop(void);
void PCM_TxPause(void);
/**
 * @brief PCM播报音频音量调节
 *
 * @param pcm_volume : 音量调节数值，调节范围为0-150
 */
void am_pcm_volume(uint8_t pcm_volume);
#endif
