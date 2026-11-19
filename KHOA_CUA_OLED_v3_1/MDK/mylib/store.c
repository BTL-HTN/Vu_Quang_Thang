#include "store.h"
#include <string.h>

#define STORE_ADDR 0x0800FC00UL  // dia chi trang Flash cuoi (page 63, 1KB)
#define STORE_MAGIC 0x4C4F434BUL // "LOCK" - danh dau Flash da co du lieu hop le

// Mat khau mac dinh lan dau chay (sau do doi trong menu admin)
#define DEFAULT_USER_PW "1234"
#define DEFAULT_ADMIN_PW "9999"

// Cau truc du lieu luu trong Flash (kich thuoc la boi so cua 4 byte)
typedef struct
{
    uint32_t magic;
    char user_pw[12];
    char admin_pw[12];
    uint32_t count;
    uint8_t uid[STORE_MAX_CARDS][4];
} Store_t;

static Store_t g_store; // ban sao trong RAM

/* ================== Phan ghi Flash bang thanh ghi ================== */
/* FLASH->SR : bit0 BSY, bit2 PGERR, bit4 WRPRTERR, bit5 EOP
   FLASH->CR : bit0 PG, bit1 PER, bit6 STRT, bit7 LOCK */

static uint8_t flash_wait(void)
{
    uint32_t t = 0x00FFFFFF;
    while ((FLASH->SR & 0x01) && --t)
    {
    } // cho BSY = 0
    if (t == 0)
        return 0;
    return 1;
}

static void flash_unlock(void)
{
    if (FLASH->CR & 0x80)
    {                               // dang bi khoa (LOCK = 1)
        FLASH->KEYR = 0x45670123UL; // khoa mo thu 1
        FLASH->KEYR = 0xCDEF89ABUL; // khoa mo thu 
    }
}

static void flash_lock(void)
{
    FLASH->CR |= 0x80; // LOCK = 1
}

// Xoa 1 trang 1KB
static uint8_t flash_erase_page(uint32_t addr)
{
    uint8_t ok;
    if (!flash_wait())
        return 0;
    FLASH->SR = 0x34;  // xoa cac co loi cu (ghi 1 de xoa)
    FLASH->CR |= 0x02; // PER = 1: chon che do xoa trang
    FLASH->AR = addr;  // dia chi trang can xoa
    FLASH->CR |= 0x40; // STRT = 1: bat dau xoa
    ok = flash_wait();
    FLASH->CR &= ~0x02; // PER = 0
    if (FLASH->SR & 0x14)
        ok = 0; // PGERR hoac WRPRTERR
    return ok;
}

// Ghi 1 half-word (16 bit) vao dia chi chan
static uint8_t flash_write_hw(uint32_t addr, uint16_t data)
{
    uint8_t ok;
    if (!flash_wait())
        return 0;
    FLASH->CR |= 0x01; // PG = 1: che do lap trinh
    *(volatile uint16_t *)addr = data;
    ok = flash_wait();
    FLASH->CR &= ~0x01; // PG = 0
    if (FLASH->SR & 0x14)
        ok = 0;
    return ok;
}

/* ================== Ham cho ben ngoai ================== */

static void store_default(void)
{
    memset(&g_store, 0, sizeof(g_store));
    g_store.magic = STORE_MAGIC;
    strcpy(g_store.user_pw, DEFAULT_USER_PW);
    strcpy(g_store.admin_pw, DEFAULT_ADMIN_PW);
    g_store.count = 0;
}

void Store_Load(void)
{
    const Store_t *p = (const Store_t *)STORE_ADDR; // doc truc tiep tu Flash

    if (p->magic == STORE_MAGIC && p->count <= STORE_MAX_CARDS)
    {
        memcpy(&g_store, p, sizeof(g_store));
        // dam bao chuoi mat khau co ky tu ket thuc
        g_store.user_pw[STORE_PW_MAX] = 0;
        g_store.admin_pw[STORE_PW_MAX] = 0;
    }
    else
    {
        store_default(); // Flash trong -> dung mac dinh
    }
}

uint8_t Store_Save(void)
{
    uint32_t i;
    uint32_t n = sizeof(g_store) / 2; // so half-word can ghi
    const uint16_t *src = (const uint16_t *)&g_store;
    uint8_t ok = 1;

    flash_unlock();
    if (!flash_erase_page(STORE_ADDR))
        ok = 0;
    if (ok)
    {
        for (i = 0; i < n; i++)
        {
            if (!flash_write_hw(STORE_ADDR + i * 2, src[i]))
            {
                ok = 0;
                break;
            }
        }
    }
    flash_lock();

    // Kiem tra lai: doc Flash so sanh voi RAM
    if (ok)
    {
        if (memcmp((const void *)STORE_ADDR, &g_store, sizeof(g_store)) != 0)
            ok = 0;
    }
    return ok;
}

int Store_FindCard(const uint8_t *uid)
{
    uint32_t i;
    for (i = 0; i < g_store.count; i++)
    {
        if (memcmp(g_store.uid[i], uid, 4) == 0)
            return (int)i;
    }
    return -1;
}

uint8_t Store_AddCard(const uint8_t *uid)
{
    if (Store_FindCard(uid) >= 0)
        return STORE_ADD_EXIST;
    if (g_store.count >= STORE_MAX_CARDS)
        return STORE_ADD_FULL;
    memcpy(g_store.uid[g_store.count], uid, 4);
    g_store.count++;
    return STORE_ADD_OK;
}

uint8_t Store_DelCard(const uint8_t *uid)
{
    int idx = Store_FindCard(uid);
    uint32_t i;
    if (idx < 0)
        return 0;
    // don cac the phia sau len 1 vi tri
    for (i = (uint32_t)idx; i + 1 < g_store.count; i++)
    {
        memcpy(g_store.uid[i], g_store.uid[i + 1], 4);
    }
    g_store.count--;
    memset(g_store.uid[g_store.count], 0, 4);
    return 1;
}

uint8_t Store_CountCards(void)
{
    return (uint8_t)g_store.count;
}

uint8_t Store_CheckPw(uint8_t who, const char *pw)
{
    const char *ref = (who == PW_ADMIN) ? g_store.admin_pw : g_store.user_pw;
    return (strcmp(ref, pw) == 0) ? 1 : 0;
}

void Store_SetPw(uint8_t who, const char *pw)
{
    char *dst = (who == PW_ADMIN) ? g_store.admin_pw : g_store.user_pw;
    memset(dst, 0, 12);
    strncpy(dst, pw, STORE_PW_MAX);
}
