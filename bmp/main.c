#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include<windows.h>

typedef struct my_bmp_s
{
    // bmp文件头
    BITMAPFILEHEADER file_header;
    // bmp信息头
    BITMAPINFOHEADER info_header;
    // 位图数据
    uint8_t* data;
    // 每一行像素数据的长度
    uint32_t line_byte;
} my_bmp_t;

my_bmp_t* my_bmp_create()
{
    my_bmp_t* bmp = (my_bmp_t*)malloc(sizeof(my_bmp_t));

    if(bmp == NULL)
    {
        fprintf(stderr, "alloc fail");
        return NULL;
    }

    memset(bmp, 0, sizeof(my_bmp_t));

    return bmp;
}

my_bmp_t* my_bmp_destroy(my_bmp_t* bmp)
{
    if(bmp == NULL)
    {
        return NULL;
    }

    if(bmp->data != NULL)
    {
        free(bmp->data);
        bmp->data = NULL;
    }

    free(bmp);

    return NULL;
}

my_bmp_t* my_bmp_read_image(const char* filename)
{
    if(filename == NULL || filename[0] == '\0')
    {
        return NULL;
    }

    FILE* fh = fopen(filename, "rb");

    if(fh == NULL)
    {
        fprintf(stderr, "Can't open bmp image file: %s\n", filename);
        return NULL;
    }

    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;

    // 读取文件头
    if( fread(&file_header, sizeof(BITMAPFILEHEADER), 1, fh) == 0 )
    {
        fclose(fh);
        fprintf(stderr, "Can't read bmp image file: %s\n", filename);
        return NULL;
    }

    // 读取信息头
    if( fread(&info_header, sizeof(BITMAPINFOHEADER), 1, fh) == 0 )
    {
        fclose(fh);
        fprintf(stderr, "Can't read bmp image file: %s\n", filename);
        return NULL;
    }

    if( file_header.bfType != 0x4d42 )
    {
        fclose(fh);
        fprintf(stderr, "Invalid bmp image file: %s\n", filename);
        return NULL;
    }

    if( info_header.biBitCount != 24 && info_header.biBitCount != 32 )
    {
        fclose(fh);
        fprintf(stderr, "Invalid bmp image file: %s\n", filename);
        return NULL;
    }

    my_bmp_t* bmp = my_bmp_create();

    if(bmp == NULL)
    {
        fclose(fh);
        return NULL;
    }

    bmp->data = (uint8_t*)malloc(info_header.biSizeImage);

    if(bmp->data == NULL)
    {
        fclose(fh);
        bmp = my_bmp_destroy(bmp);
        fprintf(stderr, "alloc fail");
        return NULL;
    }

    // 读取位图数据
    if( fread(bmp->data, 1, info_header.biSizeImage, fh) == 0 )
    {
        fclose(fh);
        bmp = my_bmp_destroy(bmp);
        fprintf(stderr, "Can't read bmp image file: %s\n", filename);
        return NULL;
    }

    bmp->file_header = file_header;
    bmp->info_header = info_header;
    bmp->line_byte = (info_header.biSizeImage / info_header.biHeight);
    //bmp->line_byte = (info_header.biWidth * (info_header.biBitCount / 8) + 3) / 4 * 4;

    fclose(fh);

    return bmp;
}

void my_bmp_graying(my_bmp_t* bmp, const char* filename)
{
    if(bmp == NULL)
    {
        return;
    }

    if(bmp->info_header.biSizeImage == 0 || bmp->data == NULL)
    {
        return;
    }

    if(filename == NULL || filename[0] == '\0')
    {
        return;
    }

    FILE* fh = fopen(filename, "wb");

    if(fh == NULL)
    {
        fprintf(stderr, "Can't open bmp image file: %s\n", filename);
        return;
    }

    BITMAPFILEHEADER gray_file_header = bmp->file_header;
    BITMAPINFOHEADER gray_info_header = bmp->info_header;

    int n_color_palette = 256;
    RGBQUAD* color_palette = (RGBQUAD*)malloc(sizeof(RGBQUAD) * n_color_palette);

    if(color_palette == NULL)
    {
        fclose(fh);
        fprintf(stderr, "alloc fail");
        return;
    }

    uint32_t gray_line_byte = (gray_info_header.biWidth * (8 / 8) + 3) / 4 * 4;
    uint32_t gray_biSizeImage = (gray_line_byte * gray_info_header.biHeight);
    uint8_t* gray_data = (uint8_t*)malloc(gray_biSizeImage);

    if(gray_data == NULL)
    {
        fclose(fh);
        free(color_palette);
        fprintf(stderr, "alloc fail");
        return;
    }

    // 修改文件头
    gray_file_header.bfSize = (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + n_color_palette * sizeof(RGBQUAD) + gray_biSizeImage);
    gray_file_header.bfOffBits = (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + n_color_palette * sizeof(RGBQUAD));

    // 修改信息头
    gray_info_header.biBitCount = 8;
    gray_info_header.biSizeImage = gray_biSizeImage;

    // 创建调色版，灰度图像的调色板是 R = G = B，而真彩图的三值不相等
    for (int i = 0; i < n_color_palette; i++)
    {
        color_palette[i].rgbRed = i;
        color_palette[i].rgbGreen = i;
        color_palette[i].rgbBlue = i;
    }

    for (uint32_t h_idx = 0; h_idx < gray_info_header.biHeight; ++h_idx)
    {
        for (uint32_t w_idx = 0; w_idx < gray_info_header.biWidth; ++w_idx)
        {
            uint8_t* src = (bmp->data + h_idx * bmp->line_byte + w_idx * 3);
            uint8_t* dst = (gray_data + h_idx * gray_line_byte + w_idx);

            // 将每一个像素都按公式 y = B * 0.299 + G * 0.587 + R * 0.114 进行转化
            *dst = (src[0] * 0.299 + src[1] * 0.587 + src[2] * 0.114);
        }
    }

    fwrite(&gray_file_header, sizeof(BITMAPFILEHEADER), 1, fh);
    fwrite(&gray_info_header, sizeof(BITMAPINFOHEADER), 1, fh);
    fwrite(color_palette, sizeof(RGBQUAD), n_color_palette, fh);
    fwrite(gray_data, 1, gray_biSizeImage, fh);

    // release resource
    fclose(fh);
    free(color_palette);
    free(gray_data);
}

void my_bmp_binaryzation(my_bmp_t* bmp, const char* filename)
{
    if(bmp == NULL)
    {
        return;
    }

    if(bmp->info_header.biSizeImage == 0 || bmp->data == NULL)
    {
        return;
    }

    if(filename == NULL || filename[0] == '\0')
    {
        return;
    }

    FILE* fh = fopen(filename, "wb");

    if(fh == NULL)
    {
        fprintf(stderr, "Can't open bmp image file: %s\n", filename);
        return;
    }

    BITMAPFILEHEADER gray_file_header = bmp->file_header;
    BITMAPINFOHEADER gray_info_header = bmp->info_header;

    int n_color_palette = 2;
    RGBQUAD* color_palette = (RGBQUAD*)malloc(sizeof(RGBQUAD) * n_color_palette);

    if(color_palette == NULL)
    {
        fclose(fh);
        fprintf(stderr, "alloc fail");
        return;
    }

    uint32_t gray_line_byte = (gray_info_header.biWidth * (8 / 8) + 3) / 4 * 4;
    uint32_t gray_biSizeImage = (gray_line_byte * gray_info_header.biHeight);
    uint8_t* gray_data = (uint8_t*)malloc(gray_biSizeImage);

    if(gray_data == NULL)
    {
        fclose(fh);
        free(color_palette);
        fprintf(stderr, "alloc fail");
        return;
    }

    // 修改文件头
    gray_file_header.bfSize = (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + n_color_palette * sizeof(RGBQUAD) + gray_biSizeImage);
    gray_file_header.bfOffBits = (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + n_color_palette * sizeof(RGBQUAD));

    // 修改信息头
    gray_info_header.biBitCount = 8;
    gray_info_header.biClrImportant = 0;
    gray_info_header.biClrUsed = 2;
    gray_info_header.biSizeImage = gray_biSizeImage;

    // 调色板白色对应的索引为150-255，黑色对应的索引为0
    color_palette[0].rgbBlue = color_palette[0].rgbGreen = color_palette[0].rgbRed = 190;
    color_palette[0].rgbReserved = 0;
    color_palette[1].rgbBlue = color_palette[1].rgbGreen = color_palette[1].rgbRed = 0;
    color_palette[1].rgbReserved = 0;

    for (uint32_t h_idx = 0; h_idx < gray_info_header.biHeight; ++h_idx)
    {
        for (uint32_t w_idx = 0; w_idx < gray_info_header.biWidth; ++w_idx)
        {
            uint8_t* src = (bmp->data + h_idx * bmp->line_byte + w_idx * 3);
            uint8_t* dst = (gray_data + h_idx * gray_line_byte + w_idx);

            // 将每一个像素都按公式 y = B * 0.299 + G * 0.587 + R * 0.114 进行转化
            uint8_t val = (src[0] * 0.299 + src[1] * 0.587 + src[2] * 0.114);

            *dst = (val >= 160) ? 1 : 0;
        }
    }

    fwrite(&gray_file_header, sizeof(BITMAPFILEHEADER), 1, fh);
    fwrite(&gray_info_header, sizeof(BITMAPINFOHEADER), 1, fh);
    fwrite(color_palette, sizeof(RGBQUAD), n_color_palette, fh);
    fwrite(gray_data, 1, gray_biSizeImage, fh);

    // release resource
    fclose(fh);
    free(color_palette);
    free(gray_data);
}

void my_bmp_print(my_bmp_t* bmp)
{
    if(bmp == NULL)
    {
        return;
    }

    printf("-------------bmp file header-------------\n");
    printf("file_header.bfType=%04x (%"PRIu16")\n", bmp->file_header.bfType, bmp->file_header.bfType);
    printf("file_header.bfSize=%08x (%"PRIu32")\n", bmp->file_header.bfSize, bmp->file_header.bfSize);
    printf("file_header.bfReserved1=%04x (%"PRIu16")\n", bmp->file_header.bfReserved1, bmp->file_header.bfReserved1);
    printf("file_header.bfReserved2=%04x (%"PRIu16")\n", bmp->file_header.bfReserved2, bmp->file_header.bfReserved2);
    printf("file_header.bfOffBits=%08x (%"PRIu32")\n", bmp->file_header.bfOffBits, bmp->file_header.bfOffBits);
    printf("-------------bmp file header-------------\n");

    printf("-------------bmp info header-------------\n");
    printf("info_header.biSize=%08x (%"PRIu32")\n", bmp->info_header.biSize, bmp->info_header.biSize);
    printf("info_header.biWidth=%08x (%"PRIu32")\n", bmp->info_header.biWidth, bmp->info_header.biWidth);
    printf("info_header.biHeight=%08x (%"PRIu32")\n", bmp->info_header.biHeight, bmp->info_header.biHeight);
    printf("info_header.biPlanes=%04x (%"PRIu16")\n", bmp->info_header.biPlanes, bmp->info_header.biPlanes);
    printf("info_header.biBitCount=%04x (%"PRIu16")\n", bmp->info_header.biBitCount, bmp->info_header.biBitCount);
    printf("info_header.biCompression=%08x (%"PRIu32")\n", bmp->info_header.biCompression, bmp->info_header.biCompression);
    printf("info_header.biSizeImage=%08x (%"PRIu32")\n", bmp->info_header.biSizeImage, bmp->info_header.biSizeImage);
    printf("info_header.biXPelsPerMeter=%08x (%"PRIu32")\n", bmp->info_header.biXPelsPerMeter, bmp->info_header.biXPelsPerMeter);
    printf("info_header.biYPelsPerMeter=%08x (%"PRIu32")\n", bmp->info_header.biYPelsPerMeter, bmp->info_header.biYPelsPerMeter);
    printf("info_header.biClrUsed=%08x (%"PRIu32")\n", bmp->info_header.biClrUsed, bmp->info_header.biClrUsed);
    printf("info_header.biClrImportant=%08x (%"PRIu32")\n", bmp->info_header.biClrImportant, bmp->info_header.biClrImportant);
    printf("-------------bmp info header-------------\n");

    printf("-------------bmp line byte-------------\n");
    printf("line_byte=%"PRIu32" (biSizeImage / biHeight = %"PRIu32" / %"PRIu32")\n", bmp->line_byte, bmp->info_header.biSizeImage, bmp->info_header.biHeight);
    printf("-------------bmp line byte-------------\n");
}

int main()
{
    my_bmp_t* bmp = my_bmp_read_image("image\\color.bmp");

    my_bmp_print(bmp);
    my_bmp_graying(bmp, "image\\graying.bmp");
    my_bmp_binaryzation(bmp, "image\\binaryzation.bmp");

    bmp = my_bmp_destroy(bmp);

    return 0;
}
