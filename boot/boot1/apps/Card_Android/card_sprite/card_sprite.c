/*
************************************************************************************************************************
*                                                         eGon2
*                                         the Embedded GO-ON Bootloader System
*
*                             Copyright(C), 2006-2008, SoftWinners Microelectronic Co., Ltd.
*											       All Rights Reserved
*
* File Name   : card_sprite.c
*
* Author      : Jerry.Wang
*
* Version     : 1.1.0
*
* Date        : 2010-3-30 8:59:10
*
* Description :
*
* Others      : None at present.
*
*
* History     :
*
*  <Author>        <time>       <version>      <description>
*
* Jerry.Wang      2010-3-30       1.1.0        build the file
*
************************************************************************************************************************
*/
#include  "card_sprite_i.h"

#define   TEST_BLK_BYTES      (512 * 1024)

#define   CARD_SPRITE_START    		0
#define   CARD_SPRITE_FLASH_INFO    3
#define   CARD_SPRITE_GET_MEM  		6
#define   CARD_SPRITE_OPEN_IMG 		9
#define   CARD_SPRITE_GET_MBR  		12
#define   CARD_SPRITE_GET_MAP  		15
#define   CARD_SPRITE_DOWN_PART     80
#define   CARD_SPRITE_DOWN_BOOT1    87
#define   CARD_SPRITE_DOWN_BOOT0    95
#define   CARD_SPRITE_FINISH   		100

//int       dynamic_data = 0;
int		  private_type = 0;
__hdle    progressbar_hd;

extern    int		android_format;

static  void  sprite_show(int ratio)
{
	boot_ui_progressbar_upgrate(progressbar_hd, ratio);
}
/*
************************************************************************************************************
*
*                                             card_sprite
*
*    �������ƣ�
*
*    �����б���
*
*    ����ֵ  ��
*
*    ˵��    ��
*
*
************************************************************************************************************
*/
__s32 card_sprite(void *mbr_i, int flash_erase, int disp_type)
{
    HIMAGEITEM  imghd = 0, imgitemhd = 0;
    __u32       item_original_size = 0, item_rest_size = 0;
    __u32		item_verify_size = 0;
    __u32       part_sector, file_sector, flash_sector;
    int         crc, mbr_count, mbr_buf_size;
    __u32		img_start;
    MBR         *tmp_mbr_cfg, *mbr_info;
    download_info  *dl_info  = NULL;
    char        *tmp_mbr_buf = NULL;
    char        *src_buf = NULL, *dest_buf = NULL;
    __u32       actual_buf_addr;
    int         ret = -1;
    int         aver_rage, sprite_ratio, pre_ratio, this_ratio;          //���ȱ�־
    unsigned    i, sprite_type;
    char		flash_info[512];

    /*����һ��1M�Ŀռ䣬���ڱ�����ʱ����*/
    /*tmp_buf��tmp_dest������1M���ݿռ䣬��ռ512K��Ŀǰ���κ�һ�����ݿ鶼���ᳬ��512K*/
    if(!disp_type)
    {
		progressbar_hd = boot_ui_progressbar_create(100, 200, 700, 280);
		boot_ui_progressbar_config(progressbar_hd, UI_BOOT_GUI_RED, UI_BOOT_GUI_GREEN, 2);
		boot_ui_progressbar_active(progressbar_hd);
	}
	//��ȡԭ�е�uboot��������
	private_fetch_from_flash();
	//NAND�豸��ʼ��
    memset(flash_info, 0, 512);
    __inf("erase flag=%d\n", flash_erase);
    ret = update_flash_hardware_scan(flash_info, flash_erase);
    if(ret == 0)
    {
    	__inf("burn nand\n");

    	sprite_type = 0;
    }
    else if(ret == 1)
    {
    	__inf("burn sdcard\n");
    	sprite_type = 1;
    }
    else
    {
    	sprite_wrn("sprite update error: fail to scan flash infomation\n");

    	goto _update_error_;
    }
    //׼��nand������Ϣ
	sprite_show(CARD_SPRITE_FLASH_INFO);
	//src_buf = (char *)sprite_malloc(1024 * 1024);
    //if(!src_buf)
    //{
    //   sprite_wrn("sprite update error: fail to get memory for tmpdata\n");
    //
    //   goto _update_error_;
    //}
    //dest_buf = src_buf + 512 * 1024;
	/* dl info ��ȡ�ڴ�ռ� */
    src_buf =  (char *)(0x48000000);
    dest_buf = (char *)(0x4C000000);
    dl_info = (download_info *)sprite_malloc(sizeof(download_info));
    if(!dl_info)
    {
        sprite_wrn("sprite update error: fail to get memory for download map\n");

        goto _update_error_;
    }
    memset(dl_info, 0, sizeof(download_info));
    sprite_show(CARD_SPRITE_GET_MEM);
    //��ȡ����MBR����Ϣ
    mbr_info = (MBR *)mbr_i;
    {
    	for(i=0;i<mbr_info->PartCount;i++)
    	{
			if((!strcmp((const char *)mbr_info->array[i].classname, "DISK")) || (!strcmp((const char *)mbr_info->array[i].classname, "disk")))
        	{
            	break;
        	}
    	}
    }
    if(i == mbr_info->PartCount)
    {
    	sprite_wrn("sprite update error: fail to find disk part\n");

        goto _update_error_;
    }
	/*��ʼ��ѹ�������в���*/
//*************************************************************************************
//*************************************************************************************
	img_start = mbr_info->array[1 + i].addrlo;
	__inf("part start = %d\n", img_start);
    imghd = Img_Open(img_start, TEST_BLK_BYTES);
    if(!imghd)       //��ʼ��
    {
        sprite_wrn("sprite update error: fail to open img\n");

        goto _update_error_;
    }
    sprite_show(CARD_SPRITE_OPEN_IMG);
//    //����Ƿ���ڶ�̬����
//    dynamic_data = 0;
//	__inf("default dynamic data not exist\n");
//	imgitemhd = Img_OpenItem(imghd, "COMMON  ", "DYNAMIC_TMP00000");
//	if(imgitemhd)
//	{
//		item_original_size = Img_GetItemSize(imghd, imgitemhd);
//		if(item_original_size >= 16)
//		{
//			dynamic_data = 1;
//			__inf("dynamic data exist\n");
//		}
//	}
//	imgitemhd = NULL;
//*************************************************************************************
//*************************************************************************************
	//��ȡMBR
	imgitemhd = Img_OpenItem(imghd, "12345678", "1234567890___mbr");
	if(!imgitemhd)
	{
		sprite_wrn("sprite update error: fail to open item for mbr\n");
        goto _update_error_;
	}
	//MBR����
	item_original_size = Img_GetItemSize(imghd, imgitemhd);
	//��ȡ�ռ����ڴ��MBR
	if(!item_original_size)
    {
        sprite_wrn("sprite update error: get mbr size fail\n");
        goto _update_error_;
    }
    mbr_buf_size = item_original_size;
	tmp_mbr_buf = (char *)sprite_malloc(item_original_size);
	if(!tmp_mbr_buf)
    {
        sprite_wrn("sprite update error: fail to get memory for mbr\n");

        goto _update_error_;
    }
	if(!Img_ReadItemData(imghd, imgitemhd, (void *)tmp_mbr_buf, item_original_size))   //����MBR����
    {
        sprite_wrn("sprite update error: fail to read data for mbr\n");

        goto _update_error_;
    }
    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;
    //���MBR�ĺϷ���
    mbr_count = ((MBR *)tmp_mbr_buf)->copy;
    __inf("mbr_count = %d\n", mbr_count);
    for(i=0;i<mbr_count;i++)
    {
    	tmp_mbr_cfg = (MBR *)(tmp_mbr_buf + i * sizeof(MBR));
		crc = calc1_crc32((void *)&tmp_mbr_cfg->version, sizeof(MBR) - 4);
		__inf("count crc=%x, source crc=%x\n", crc, tmp_mbr_cfg->crc32);
		if(crc != tmp_mbr_cfg->crc32)
		{
			__wrn("sprite update warning: check mbr %d part correct fail\n", i);
			__wrn("now fix it automatically\n");
			//�Զ�����CRC
			tmp_mbr_cfg->crc32 = crc;
		}
	}
	//������������MBR��UDISK������С
	sprite_show(CARD_SPRITE_GET_MBR);
//*************************************************************************************
//*************************************************************************************
	//��ȡ DOWNLOAD MAP
	imgitemhd = Img_OpenItem(imghd, "12345678", "1234567890dlinfo");
	if(!imgitemhd)
	{
		sprite_wrn("sprite update error: fail to open item for download map\n");

        goto _update_error_;
	}
	//DOWNLOAD MAP ����
	item_original_size = Img_GetItemSize(imghd, imgitemhd);
	if(!item_original_size)
    {
        sprite_wrn("sprite update error: get download map size fail\n");
        goto _update_error_;
    }
	if(!Img_ReadItemData(imghd, imgitemhd, (void *)dl_info, item_original_size))   //���� DOWNLOAD MAP ����
    {
        sprite_wrn("sprite update error: fail to read data for download map\n");

        goto _update_error_;
    }
    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;
    //��� DOWNLOAD MAP�ĺϷ���
	crc = calc1_crc32(&dl_info->version, sizeof(download_info) - 4);
	if(crc != dl_info->crc32)
	{
		sprite_wrn("sprite update error: download map file is not correct\n");

		goto _update_error_;
	}
	sprite_show(CARD_SPRITE_GET_MAP);
/*****************************************************************************
*
*   ������д��map�����ҵ�ÿ������Ӧ��д��λ��
*
*****************************************************************************/
	update_flash_init();
	flash_sector = NAND_GetDiskSize();
	if(dl_info->download_count > 0)
	{
		aver_rage = ((CARD_SPRITE_DOWN_PART - CARD_SPRITE_GET_MAP)/dl_info->download_count);
	}
	__inf("download part count = %d\n", dl_info->download_count);
	sprite_ratio = CARD_SPRITE_GET_MAP;
	for(i=0;i<dl_info->download_count;i++)
	{
		__inf("dl name = %s\n", dl_info->one_part_info[i].name);
	    imgitemhd = Img_OpenItem(imghd, "RFSFAT16", (char *)dl_info->one_part_info[i].dl_filename);
	    if(!imgitemhd)
	    {
	        sprite_wrn("sprite update error: fail to open part file %s\n", dl_info->one_part_info[i].dl_filename);

	        goto _update_error_;
	    }
	    private_type = 0;
	    item_original_size = Img_GetItemSize(imghd, imgitemhd);
	    __inf("item_original_size=%d\n", item_original_size);
	    if(!item_original_size)
	    {
	        sprite_wrn("sprite update error: get part file %s size failed\n", dl_info->one_part_info[i].dl_filename);

	        goto _update_error_;
	    }
	    if(!strcmp((char *)dl_info->one_part_info[i].name, "env"))
	    {
	    	private_type = 1;
	    }
	    else if(!strcmp((char *)dl_info->one_part_info[i].name, "private"))
	    {
	    	continue;
	    }
	    else if(!strcmp((char *)dl_info->one_part_info[i].name, "UDISK"))
	    {
	    	dl_info->one_part_info[i].lenlo = flash_sector - dl_info->one_part_info[i].addrlo;
	    	if(item_original_size <= 2048)
	    	{
	    		continue;
	    	}
	    }
	    //sprite_wrn("get part file %s size = %d\n", dl_info->one_part_info[i].dl_filename, item_original_size);
	    item_rest_size = item_original_size;
	    //����ļ���С�Ƿ�С�ڵ��ڷ�����С
		file_sector = item_original_size / 512;
		part_sector = dl_info->one_part_info[i].lenlo;
		if(file_sector > part_sector)
		{
			sprite_wrn("sprite update error: file size is %d, part size is %d, cant burn part %s\n", file_sector, part_sector, dl_info->one_part_info[i].dl_filename);

			goto _update_error_;
		}
	    update_flash_open(dl_info->one_part_info[i].addrlo, dl_info->one_part_info[i].addrhi);
	    //��ʼ�����ܺ���
	    init_code(dl_info->one_part_info[i].encrypt);
	    while(item_rest_size >= TEST_BLK_BYTES)      //������32k��ʱ��
	    {
	    	if(!Img_ReadItemData(imghd, imgitemhd, (void *)src_buf, TEST_BLK_BYTES))   //����32k����
	        {
	            sprite_wrn("sprite update error: fail to read data from %s\n", dl_info->one_part_info[i].dl_filename);

	            goto _update_error_;
	        }
	        item_rest_size -= TEST_BLK_BYTES;
	        decode((__u32)src_buf, (__u32)dest_buf, TEST_BLK_BYTES, &actual_buf_addr);
	        if(update_flash_write((void *)actual_buf_addr, TEST_BLK_BYTES))
	        {
	        	sprite_wrn("sprite update error: fail to write data in %s\n", dl_info->one_part_info[i].dl_filename);

	            goto _update_error_;
	        }
	        this_ratio = (((item_original_size>>9) - (item_rest_size>>9))*aver_rage)/(item_original_size>>9);
	        if(this_ratio)
	        {
	        	if(pre_ratio != this_ratio)
	        	{
	        		sprite_show(sprite_ratio + this_ratio);
	    			pre_ratio = this_ratio;
	    			__inf("this ratio = %d\n", this_ratio);
	    		}
	    	}
	    }
	    if(item_rest_size)
	    {
	        if(!Img_ReadItemData(imghd, imgitemhd, (void *)src_buf, item_rest_size))   //����32k����
	        {
	            sprite_wrn("sprite update error: fail to read data from %s\n", dl_info->one_part_info[i].dl_filename);

	            goto _update_error_;
	        }
	        decode((__u32)src_buf, (__u32)dest_buf, item_rest_size, &actual_buf_addr);
	        if(update_flash_write((void *)actual_buf_addr, item_rest_size))
	        {
	        	sprite_wrn("sprite update error: fail to write last data in %s\n", dl_info->one_part_info[i].dl_filename);

	            goto _update_error_;
	        }
	    }
	    exit_code();
	    __inf("data deal finish\n");
	    update_force_to_flash();
	    sprite_ratio += aver_rage;
	    __msg("sprite_ratio = %d\n", sprite_ratio);
	    sprite_show(sprite_ratio);
	    //��ʱ��ʼ��У��
	    if(private_type)
	    {
	    	continue;
	    }
	    if(dl_info->one_part_info[i].vf_filename[0])
	    {
	    	imgitemhd = Img_OpenItem(imghd, "RFSFAT16", (char *)dl_info->one_part_info[i].vf_filename);
	    	if(!imgitemhd)
	    	{
	        	__inf("sprite update warning: cant open verify file %s\n", dl_info->one_part_info[i].vf_filename);
	    	}
	    	else
	    	{
	    		item_verify_size = Img_GetItemSize(imghd, imgitemhd);
	    		if(!item_verify_size)
			    {
			        __inf("sprite update warning: cant get verify file %s size\n", dl_info->one_part_info[i].vf_filename);
				}
				else
				{
					init_code(0);
					if(!Img_ReadItemData(imghd, imgitemhd, (void *)src_buf, item_verify_size))   //��������
			        {
			            __inf("sprite update warning: fail to read data from %s\n", dl_info->one_part_info[i].vf_filename);
			        }
			        else
			        {
			        	__u32 check_sum = *(__u32 *)src_buf;
			        	__u32 sum = 0;

						if(android_format == -1)
						{
				        	//����flash�ϵ�����
				        	__inf("normal type verify\n");
				        	item_rest_size = item_original_size;
				        	update_flash_open(dl_info->one_part_info[i].addrlo, dl_info->one_part_info[i].addrhi);
				        	while(item_rest_size >= 512 * 1024)
				        	{
				        		if(update_flash_read_ext(dest_buf, 512 * 1024))
				        		{
				        			__inf("sprite update warning: fail to read data in %s\n", dl_info->one_part_info[i].vf_filename);

				        			goto __download_part_data__;
				        		}
								sum += verify_sum(dest_buf, 512 * 1024);
								item_rest_size -= 512 * 1024;
				        	}
				        	if(item_rest_size)
				        	{
				        		if(update_flash_read_ext(dest_buf, item_rest_size))
				        		{
				        			__inf("sprite update warning: fail to read data in %s\n", dl_info->one_part_info[i].vf_filename);

				        			goto __download_part_data__;
				        		}
				        		sum += verify_sum(dest_buf, item_rest_size);
				        	}
				        }
				        else
				        {
				        	__inf("sparse type verify\n");
				        	sum = unsparse_checksum();
				        }
				        __inf("pc sum=%x, check sum=%x\n", sum, check_sum);
				        if(sum != check_sum)
			        	{
			        		sprite_wrn("sprite update error: checksum is error %s\n", dl_info->one_part_info[i].dl_filename);

			        		goto _update_error_;
			        	}
				    }
				}
	    	}
	    }
	    else
	    {
	    	__inf("part %s not need verify\n", dl_info->one_part_info[i].dl_filename);
	    }
__download_part_data__:
		;
	}
	sprite_show(CARD_SPRITE_DOWN_PART);
/*****************************************************************************
*
*   ��ԭ����������
*
*   private��������
*
*
*****************************************************************************/
	__inf("try to deal private part\n");
	for(i=0;i<mbr_info->PartCount;i++)
	{
		if(!strcmp((const char *)mbr_info->array[i].name, "private"))
		{
			__u32 private_start, private_size;

			private_start = mbr_info->array[i].addrlo;
			private_size  = mbr_info->array[i].lenlo;

			update_flash_open(private_start, 0);
			if(private_date_restore(private_size<<9))
			{
				sprite_wrn("sprite update error: fail to write private date\n");
			}
			update_flash_close();

        	break;
    	}
	}
	__inf("dela private part finish\n");
/*****************************************************************************
*
*   ��ʼ��mbr���в���
*
*   mbrֱ��д���߼�����0�ſ�ʼ����
*
*
*****************************************************************************/
	update_flash_open(0, 0);
	private_type = 0;
	__msg("write mbr, size = %d\n", mbr_buf_size);
	if(update_flash_write((void *)tmp_mbr_buf, mbr_buf_size))
	{
    	sprite_wrn("sprite update error: fail to write mbr\n");

        goto _update_error_;
    }
    __inf("sprite_type = %d\n", sprite_type);
    if(sprite_type)
    {
    	__inf("write standard mbr\n");
    	if(create_stdmbr((void *)tmp_mbr_buf))
    	{
    		__msg("write standard mbr failed\n");
    	}
    }
    update_flash_close();
    if(!sprite_type)
    {
    	update_flash_exit(0);
	}
	sprite_show(CARD_SPRITE_DOWN_PART);
/*****************************************************************************
*
*   ��ʼ��boot1���в�����������ȡ�������ȡ����
*
*   boot1��ѹ���������ݰ�����Ҫ���н�ѹ
*
*   boot1���Ա�����dram��
*
*****************************************************************************/
    //ȡ��boot1
    if(!sprite_type)
    {
    	imgitemhd = Img_OpenItem(imghd, "BOOT    ", "BOOT1_0000000000");
    }
    else
    {
    	imgitemhd = Img_OpenItem(imghd, "12345678", "1234567890boot_1");
    }
    if(!imgitemhd)
    {
        sprite_wrn("sprite update error: fail to open boot1 item\n");
        goto _update_error_;
    }
    //boot1����
    item_original_size = Img_GetItemSize(imghd, imgitemhd);
    if(!item_original_size)
    {
        sprite_wrn("sprite update error: fail to get boot1 item size\n");
        goto _update_error_;
    }
    /*��ȡboot1������(ѹ������)*/
    if(!Img_ReadItemData(imghd, imgitemhd, (void *)src_buf, item_original_size))
    {
        sprite_wrn("update error: fail to read data from for boot1\n");
        goto _update_error_;
    }
    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;
    if(!sprite_type)
    {
    	init_code(1);
    }
    else
    {
    	init_code(0);
    }
    decode((__u32)src_buf, (__u32)dest_buf, item_original_size, &actual_buf_addr);
    exit_code();

    if(update_boot1((void *)actual_buf_addr, flash_info, sprite_type))
    {
    	sprite_wrn("update error: fail to write boot1\n");
        goto _update_error_;
    }
	sprite_show(CARD_SPRITE_DOWN_BOOT1);
/*****************************************************************************
*
*   ��ʼ��boot0���в�����������ȡ�������ȡ����
*
*   boot0��ѹ���������ݰ�����Ҫ���н�ѹ
*
*   boot0���Ա�����dram��
*
*****************************************************************************/
    if(!sprite_type)
    {
    	imgitemhd = Img_OpenItem(imghd, "BOOT    ", "BOOT0_0000000000");
    }
    else
    {
    	imgitemhd = Img_OpenItem(imghd, "12345678", "1234567890boot_0");
    }
    if(!imgitemhd)
    {
        sprite_wrn("sprite update error: fail to open boot0 item\n");
        goto _update_error_;
    }
    item_original_size = Img_GetItemSize(imghd, imgitemhd);
    if(!item_original_size)
    {
        sprite_wrn("sprite update error: fail to get boot0 size\n");
        goto _update_error_;
    }
    if(!Img_ReadItemData(imghd, imgitemhd, (void *)src_buf, item_original_size))
    {
        sprite_wrn("sprite update error: fail to get boot0 item size\n");
        goto _update_error_;
    }
    Img_CloseItem(imghd, imgitemhd);
    imgitemhd = NULL;
    if(!sprite_type)
    {
    	init_code(1);
    }
    else
    {
    	init_code(0);
    }
    decode((__u32)src_buf, (__u32)dest_buf, item_original_size, &actual_buf_addr);
    exit_code();

    if(update_boot0((void *)actual_buf_addr, flash_info, sprite_type))
    {
    	sprite_wrn("update error: fail to write boot0\n");
        goto _update_error_;
    }
	sprite_show(CARD_SPRITE_DOWN_BOOT0);
/*****************************************************************************
*
*   �ر�imghd��nand���
*
*
*****************************************************************************/
    Img_Close(imghd);

    ret = 0;

_update_error_:
//    if(src_buf)
//    {
//        sprite_free(src_buf);
//    }
//    if(buf0)
//    {
//        sprite_free(buf0);
//    }
//    if(buf1)
//    {
//        sprite_free(buf1);
//    }
    if(tmp_mbr_buf)
    {
    	sprite_free(tmp_mbr_buf);
    }
    if(sprite_type)
    {
    	update_flash_exit(1);
    }
    if(!ret)
    {
    	sprite_show(CARD_SPRITE_FINISH);
    }
    else
    {
		sprite_wrn("sprite update error: current card sprite failed\n");
		sprite_wrn("now hold the machine\n");
    }

    return ret;
}
