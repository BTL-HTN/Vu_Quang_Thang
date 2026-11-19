

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
#include "i2c.h"
#include "OLED.h"
#include "keybad.h"
#include "sever.h"
#include "spi.h"
#include "rc522.h"
#include "store.h"
#include "led.h"

/* ------------------------- CAU HINH OLED ------------------------- */
#define OLED_I2C 1   // OLED dung I2C1 (PB6 = SCL, PB7 = SDA)
#define OLED_X 16    // cot bat dau ve chu (16 ky tu x 6 diem = 96 diem -> can giua man 128)
#define OLED_PAGE0 2 // dong 1 nam o page 2 (man hinh co 8 page 0..7, moi page cao 8 diem)
#define OLED_PAGE1 5 // dong 2 nam o page 5

/* ------------------------- CAU HINH CHUNG ------------------------- */
#define SERVO_GOC_DONG 0 // goc khi cua dong (do)
#define SERVO_GOC_MO 90  // goc khi cua mo (do) - chinh lai cho dung co cau cua ban

#define THOI_GIAN_MO_MS 3000    // giu cua mo bao lau
#define SO_LAN_SAI_TOI_DA 3     // sai qua so lan nay se bi khoa tam thoi
#define THOI_GIAN_KHOA_MS 10000 // thoi gian khoa tam thoi

#define TIMEOUT_NHAP_MS 10000 // het thoi gian nhap mat khau -> ve man hinh cho
#define TIMEOUT_MENU_MS 30000 // het thoi gian o menu admin -> ve man hinh cho

#define HIEN_PHIM_DEBUG 1          // 1 = hien "Phim: X" o dong cuoi OLED de kiem tra ban phim (dat 0 de tat)
#define CHO_PHEP_DANG_KY_THE_DAU 1 // 1 = khi CHUA CO THE NAO, the quet dau tien duoc dang ky luon (dat 0 de tat)

/* Loai su kien gui tu cac task nhap vao Task_Access */
#define EV_KEY 1
#define EV_CARD 2

typedef struct
{
    uint8_t type;   // EV_KEY hoac EV_CARD
    uint8_t key;    // ky tu phim (neu EV_KEY)
    uint8_t uid[4]; // UID the (neu EV_CARD)
} Event_t;

typedef struct
{
    char l0[17]; // dong 1 cua OLED (16 ky tu + '\0')
    char l1[17]; // dong 2 cua OLED
} LcdMsg_t;

/* Cac trang thai cua he thong */
typedef enum
{
    ST_IDLE = 0, // man hinh cho
    ST_PASS,     // dang nhap mat khau
    ST_MENU,     // menu admin
    ST_ADD,      // cho quet the de THEM
    ST_DEL,      // cho quet the de XOA
    ST_CHPW_SEL, // chon doi mat khau user hay admin
    ST_CHPW_NEW  // nhap mat khau moi
} State_t;

/* ------------------------- BIEN TOAN CUC ------------------------- */
static QueueHandle_t xEventQueue; // su kien: phim + the
static QueueHandle_t xLcdQueue;   // noi dung can hien thi len OLED

static volatile uint16_t g_led_ms = 0; // thoi gian LED con phai nhay (ms), Task_Access ghi, Task_Display doc

static volatile uint8_t g_rc522_ok = 0; // 1 = RC522 phan hoi binh thuong
static volatile uint8_t g_last_key = 0; // phim vua bam (Task_Keypad ghi, Task_Display hien thi)

static State_t g_state;
static char g_pw[STORE_PW_MAX + 1]; // mat khau dang nhap
static uint8_t g_pw_len;            // độ dài của mật khẩu
static uint8_t g_chpw_target;       // doi mat khau cua ai: PW_USER / PW_ADMIN
static uint8_t g_fail_count;        // so lan sai lien tiep

/* ------------------------- HAM HO TRO ------------------------- */

// Gui 2 dong chu sang Task_Display (tu dong them dau cach cho du 16 ky tu)
static void Lcd_Show(const char *a, const char *b)
{
    LcdMsg_t m;
    uint8_t i;

    for (i = 0; i < 16; i++)
    {
        if (*a != 0)
        {
            m.l0[i] = *a;
            a++;
        }
        else
        {
            m.l0[i] = ' ';
        }
        if (*b != 0)
        {
            m.l1[i] = *b;
            b++;
        }
        else
        {
            m.l1[i] = ' ';
        }
    }
    m.l0[16] = 0;
    m.l1[16] = 0;
    xQueueSend(xLcdQueue, &m, 0); // khong cho: neu hang doi day thi bo qua, KHONG lam dung Task_Access
}

// Yeu cau LED nhay trong 'ms' mili giay (Task_Display se thuc hien)
static void Led_Alarm(uint16_t ms)
{
    g_led_ms = ms;
}

// Doi so 0..99 thanh chuoi (dung hien thi so the)
static void Num2Str(uint8_t n, char *out)
{
    if (n >= 10)
    {
        *out++ = (char)('0' + n / 10);
    }
    *out++ = (char)('0' + n % 10);
    *out = 0;
}

static void Show_Idle(void)
{
    Lcd_Show("  KHOA CUA RFID ", "Quet the/Nhap MK");
}

static void Show_Menu(void)
{
    Lcd_Show("1.Them the 2.Xoa", "3.Doi MK D.Thoat");
}

// Hien thi dau '*' tuong ung so ky tu da nhap
static void Show_Pass(const char *title)
{
    char stars[17];
    uint8_t i;
    for (i = 0; i < g_pw_len; i++)
        stars[i] = '*';
    stars[g_pw_len] = 0;
    Lcd_Show(title, stars);
}

static void Show_ChPw(void)
{
    Show_Pass(g_chpw_target == PW_ADMIN ? "MK Admin moi:" : "MK User moi:");
}

static void Flush_Events(void)
{
    xQueueReset(xEventQueue); // bo cac su kien tich luy trong luc dang ban
}

// Mo cua: servo mo -> giu -> dong lai
static void Do_OpenDoor(const char *msg)
{
    g_fail_count = 0;
    Lcd_Show(msg, "CUA DANG MO");
    Servo_Write(SERVO_GOC_MO);
    vTaskDelay(pdMS_TO_TICKS(THOI_GIAN_MO_MS));

    Lcd_Show("DONG CUA...", "");
    Servo_Write(SERVO_GOC_DONG);
    vTaskDelay(pdMS_TO_TICKS(1000)); // cho servo quay ve het

    Flush_Events();
}

// Sai the / sai mat khau: bao loi + LED, sai nhieu lan thi khoa tam thoi
static void Do_Wrong(const char *msg)
{
    g_fail_count++;
    if (g_fail_count >= SO_LAN_SAI_TOI_DA)
    {
        Lcd_Show("BI KHOA TAM THOI", "Cho 10 giay...");
        Led_Alarm(THOI_GIAN_KHOA_MS);
        vTaskDelay(pdMS_TO_TICKS(THOI_GIAN_KHOA_MS));
        g_fail_count = 0;
    }
    else
    {
        Lcd_Show(msg, "Thu lai...");
        Led_Alarm(2000);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    Flush_Events();
}

// Xu ly the quet o man hinh cho / dang nhap: the hop le -> mo cua
static void Handle_CardAccess(const uint8_t *uid)
{
#if CHO_PHEP_DANG_KY_THE_DAU
    if (Store_CountCards() == 0)
    { // chua co the nao: dang ky luon the dau tien
        if (Store_AddCard(uid) == STORE_ADD_OK && Store_Save())
        {
            Lcd_Show("DA DANG KY THE", "THE DAU TIEN");
        }
        else
        {
            Lcd_Show("LOI GHI FLASH", "");
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
        Flush_Events();
        g_pw_len = 0;
        g_state = ST_IDLE;
        Show_Idle();
        return;
    }
#endif
    if (Store_FindCard(uid) >= 0)
    {
        Do_OpenDoor("THE HOP LE");
    }
    else
    {
        Do_Wrong("THE KHONG HOP LE");
    }
    g_pw_len = 0;
    g_state = ST_IDLE;
    Show_Idle();
}

// Menu admin: them the
static void Handle_AddCard(const uint8_t *uid)
{
    uint8_t r = Store_AddCard(uid);
    char num[4];
    char line[17];

    if (r == STORE_ADD_OK)
    {
        if (Store_Save())
        {
            Num2Str(Store_CountCards(), num);
            strcpy(line, "So the: ");
            strcat(line, num);
            Lcd_Show("DA THEM THE", line);
        }
        else
        {
            Lcd_Show("LOI GHI FLASH", "");
        }
    }
    else if (r == STORE_ADD_EXIST)
    {
        Lcd_Show("THE DA TON TAI", "");
    }
    else
    {
        Lcd_Show("DANH SACH DAY", "");
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
    Flush_Events();
    g_state = ST_MENU;
    Show_Menu();
}

// Menu admin: xoa the
static void Handle_DelCard(const uint8_t *uid)
{
    char num[4];
    char line[17];

    if (Store_DelCard(uid))
    {
        if (Store_Save())
        {
            Num2Str(Store_CountCards(), num);
            strcpy(line, "So the: ");
            strcat(line, num);
            Lcd_Show("DA XOA THE", line);
        }
        else
        {
            Lcd_Show("LOI GHI FLASH", "");
        }
    }
    else
    {
        Lcd_Show("KHONG TIM THAY", "");
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
    Flush_Events();
    g_state = ST_IDLE;
    Show_Idle();
}

// Them / xoa 1 ky tu trong bo dem mat khau. Tra ve 1 neu da xu ly (la so hoac '*')
static uint8_t Pw_EditKey(uint8_t key)
{
    if (key >= '0' && key <= '9')
    {
        if (g_pw_len < STORE_PW_MAX)
        {
            g_pw[g_pw_len] = (char)key;
            g_pw_len++;
            g_pw[g_pw_len] = 0;
        }
        return 1;
    }
    if (key == '*')
    {
        if (g_pw_len > 0)
        {
            g_pw_len--;
            g_pw[g_pw_len] = 0;
        }
        return 1;
    }
    return 0;
}

// Xac nhan mat khau dang nhap (nhan '#')
static void Pw_Confirm(void)
{
    if (Store_CheckPw(PW_ADMIN, g_pw))
    {
        g_fail_count = 0;
        g_pw_len = 0;
        g_pw[0] = 0;
        g_state = ST_MENU;
        Lcd_Show("ADMIN DANG NHAP", "");
        vTaskDelay(pdMS_TO_TICKS(800));
        Flush_Events();
        Show_Menu();
    }
    else if (Store_CheckPw(PW_USER, g_pw))
    {
        Do_OpenDoor("MAT KHAU DUNG");
        g_pw_len = 0;
        g_pw[0] = 0;
        g_state = ST_IDLE;
        Show_Idle();
    }
    else
    {
        Do_Wrong("SAI MAT KHAU");
        g_pw_len = 0;
        g_pw[0] = 0;
        g_state = ST_IDLE;
        Show_Idle();
    }
}

// Xac nhan mat khau moi (nhan '#' trong che do doi mat khau)
static void Pw_ConfirmNew(void)
{
    uint8_t other = (g_chpw_target == PW_ADMIN) ? PW_USER : PW_ADMIN;

    if (g_pw_len < STORE_PW_MIN)
    {
        Lcd_Show("TOI THIEU 4 SO", "");
        vTaskDelay(pdMS_TO_TICKS(1200));
        Flush_Events();
        Show_ChPw();
        return;
    }
    if (Store_CheckPw(other, g_pw))
    { // khong cho trung mat khau cua loai kia
        Lcd_Show("TRUNG MK KIA", "Nhap MK khac");
        vTaskDelay(pdMS_TO_TICKS(1500));
        Flush_Events();
        Show_ChPw();
        return;
    }
    Store_SetPw(g_chpw_target, g_pw);
    if (Store_Save())
    {
        Lcd_Show("DA DOI MAT KHAU", "");
    }
    else
    {
        Lcd_Show("LOI GHI FLASH", "");
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
    Flush_Events();
    g_pw_len = 0;
    g_pw[0] = 0;
    g_state = ST_MENU;
    Show_Menu();
}

/* ===================== TASK 1: BAN PHIM ===================== */
static void Task_Keypad(void *pvParameters)
{
    Event_t ev;
    char k;
    uint8_t n;
    (void)pvParameters;

    memset(&ev, 0, sizeof(ev));
    ev.type = EV_KEY;

    // Bo qua nhieu luc khoi dong (phim "ma"): doi 300ms, roi doi cho toi khi KHONG con phim nao dang nhan
    vTaskDelay(pdMS_TO_TICKS(300));
    for (n = 0; n < 50 && Keypad_Scan() != 0; n++)
    {
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    while (1)
    {
        k = Keypad_GetKey(); // tra ve phim 1 lan moi lan nhan (da chong rung)
        if (k != 0)
        {
            ev.key = (uint8_t)k;
            g_last_key = (uint8_t)k; // de Task_Display hien thi (kiem tra ban phim)
            xQueueSend(xEventQueue, &ev, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // quet 20ms/lan
    }
}

/* ===================== TASK 2: RFID ===================== */
static void Task_RFID(void *pvParameters)
{
    Event_t ev;
    uint8_t uid[4];
    uint8_t chk = 0;
    uint8_t ver;
    (void)pvParameters;

    memset(&ev, 0, sizeof(ev));
    ev.type = EV_CARD;

    while (1)
    {
        if (!g_rc522_ok)
        {
            g_rc522_ok = RC522_Init(); // khoi tao lai neu chua thay chip
            if (!g_rc522_ok)
            {
                vTaskDelay(pdMS_TO_TICKS(2000)); // khong thay RC522, thu lai sau 2 giay
                continue;
            }
        }

        if (RC522_Request() == RC522_OK)
        { // co the o gan
            if (RC522_Anticoll(uid) == RC522_OK)
            {                 // doc duoc UID
                RC522_Halt(); // the vao HALT: khong doc lap lai den khi rut ra
                memcpy(ev.uid, uid, 4);
                xQueueSend(xEventQueue, &ev, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }

        // Moi ~5 giay kiem tra chip con song khong (phat hien tuot day)
        chk++;
        if (chk >= 50)
        {
            chk = 0;
            ver = RC522_ReadVersion();
            if (ver == 0x00 || ver == 0xFF)
                g_rc522_ok = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // hoi the 10 lan/giay
    }
}

/* ===================== TASK 3: XU LY TRUNG TAM ===================== */
static void Task_Access(void *pvParameters)
{
    Event_t ev;
    TickType_t wait;
    uint8_t k;
    (void)pvParameters;

    g_state = ST_IDLE;
    g_pw_len = 0;
    g_pw[0] = 0;
    g_fail_count = 0;

    // --- Man hinh khoi dong, hien thi ket qua kiem tra RC522 ---
    Lcd_Show("KHOI DONG...", "");
    Led_Alarm(300);
    vTaskDelay(pdMS_TO_TICKS(1500)); // cho Task_RFID khoi tao xong RC522
    if (g_rc522_ok)
        Lcd_Show("RC522: OK", "");
    else
        Lcd_Show("RC522: LOI NOI", "Kiem tra day SPI");
    vTaskDelay(pdMS_TO_TICKS(1500));
    Flush_Events(); // bo phim/the tich luy luc khoi dong, chi nhan tu bay gio
    g_last_key = 0;
    Show_Idle();

    while (1)
    {
        // Man hinh cho thi doi vo han, cac trang thai khac co timeout
        switch (g_state)
        {
        case ST_IDLE:
            wait = portMAX_DELAY;
            break;
        case ST_PASS:
            wait = pdMS_TO_TICKS(TIMEOUT_NHAP_MS);
            break;
        default:
            wait = pdMS_TO_TICKS(TIMEOUT_MENU_MS);
            break;
        }

        if (xQueueReceive(xEventQueue, &ev, wait) != pdPASS)
        {
            // Het thoi gian khong thao tac -> ve man hinh cho
            g_state = ST_IDLE;
            g_pw_len = 0;
            g_pw[0] = 0;
            Show_Idle();
            continue;
        }

        /* ---------- SU KIEN THE RFID ---------- */
        if (ev.type == EV_CARD)
        {
            if (g_state == ST_IDLE || g_state == ST_PASS)
            {
                Handle_CardAccess(ev.uid);
            }
            else if (g_state == ST_ADD)
            {
                Handle_AddCard(ev.uid);
            }
            else if (g_state == ST_DEL)
            {
                Handle_DelCard(ev.uid);
            }
            // cac trang thai khac: bo qua the
            continue;
        }

        /* ---------- SU KIEN PHIM ---------- */
        k = ev.key;
        switch (g_state)
        {

        case ST_IDLE:
            if (k >= '0' && k <= '9')
            { // bam so dau tien -> vao che do nhap mat khau
                g_pw_len = 0;
                g_pw[0] = 0;
                Pw_EditKey(k);
                g_state = ST_PASS;
                Show_Pass("Nhap mat khau:");
            }
            break;

        case ST_PASS:
            if (Pw_EditKey(k))
            {
                if (g_pw_len == 0)
                { // xoa het -> ve man hinh cho
                    g_state = ST_IDLE;
                    Show_Idle();
                }
                else
                {
                    Show_Pass("Nhap mat khau:");
                }
            }
            else if (k == '#')
            {
                Pw_Confirm();
            }
            else if (k == 'D')
            { // huy
                g_pw_len = 0;
                g_pw[0] = 0;
                g_state = ST_IDLE;
                Show_Idle();
            }
            break;

        case ST_MENU:
            if (k == '1')
            {
                g_state = ST_ADD;
                Lcd_Show("THEM THE MOI", "Quet the.. D:Huy");
            }
            else if (k == '2')
            {
                g_state = ST_DEL;
                Lcd_Show("XOA THE", "Quet the.. D:Huy");
            }
            else if (k == '3')
            {
                g_state = ST_CHPW_SEL;
                Lcd_Show("DOI MAT KHAU", "1.User  2.Admin");
            }
            else if (k == 'D')
            {
                g_state = ST_IDLE;
                Show_Idle();
            }
            break;

        case ST_ADD:
        case ST_DEL:
            if (k == 'D')
            { // huy, ve menu
                g_state = ST_MENU;
                Show_Menu();
            }
            break;

        case ST_CHPW_SEL:
            if (k == '1' || k == '2')
            {
                g_chpw_target = (k == '1') ? PW_USER : PW_ADMIN;
                g_pw_len = 0;
                g_pw[0] = 0;
                g_state = ST_CHPW_NEW;
                Show_ChPw();
            }
            else if (k == 'D')
            {
                g_state = ST_MENU;
                Show_Menu();
            }
            break;

        case ST_CHPW_NEW:
            if (Pw_EditKey(k))
            {
                Show_ChPw();
            }
            else if (k == '#')
            {
                Pw_ConfirmNew();
            }
            else if (k == 'D')
            { // huy, ve menu
                g_pw_len = 0;
                g_pw[0] = 0;
                g_state = ST_MENU;
                Show_Menu();
            }
            break;

        default:
            g_state = ST_IDLE;
            break;
        }
    }
}

/* ===================== TASK 4: HIEN THI (OLED + LED) ===================== */
// Chi Task_Display duoc ghi OLED/I2C -> tranh tranh chap giua cac task
// Moi 100ms: neu g_led_ms > 0 thi dao LED, het thoi gian thi tat LED
static void Task_Display(void *pvParameters)
{
    LcdMsg_t m;
    TickType_t last = 0;
    uint8_t shown_key = 0;
    char kline[17];
    (void)pvParameters;

    // Doi nguon OLED on dinh roi khoi tao OLED (chi Task_Display duoc dung I2C1 de ve man hinh)
    vTaskDelay(pdMS_TO_TICKS(100));
    oled_init(OLED_I2C);
    oled_blank(OLED_I2C);

    while (1)
    {
        // Cho noi dung OLED toi da 100ms (het gio van chay tiep de nhay LED)
        if (xQueueReceive(xLcdQueue, &m, pdMS_TO_TICKS(100)) == pdPASS)
        {
            while (xQueueReceive(xLcdQueue, &m, 0) == pdPASS)
            {
            } // bo noi dung cu, chi ve noi dung MOI NHAT
            oled_msg(OLED_I2C, OLED_PAGE0, OLED_X, m.l0);
            oled_msg(OLED_I2C, OLED_PAGE1, OLED_X, m.l1);
        }

#if HIEN_PHIM_DEBUG
        // Dong cuoi (page 7): hien phim vua bam -> nhin thay ngay ban phim co nhan phim do khong
        if (g_last_key != shown_key)
        {
            shown_key = g_last_key;
            memset(kline, ' ', 16);
            memcpy(kline, "Phim:", 5);
            kline[6] = (char)shown_key;
            kline[16] = 0;
            oled_msg(OLED_I2C, 7, OLED_X, kline);
        }
#endif

        // Moi 100ms xu ly LED 1 lan
        if ((xTaskGetTickCount() - last) >= pdMS_TO_TICKS(100))
        {
            last = xTaskGetTickCount();
            if (g_led_ms > 0)
            {
                Led_Toggle();
                if (g_led_ms > 100)
                {
                    g_led_ms -= 100;
                }
                else
                {
                    g_led_ms = 0;
                    Led_Off();
                }
            }
        }
    }
}

/* ===================== BAO LOI HE THONG BANG LED PC14 =====================
   Neu chip bi loi nang (truoc day chi dung im, khong biet vi sao), LED se bao:
     - Chop RAT NHANH (~5 lan/giay) : HardFault (loi truy cap bo nho / con tro sai)
     - SANG LIEN TUC                : tran stack cua 1 task
     - Nhay CHAM (~1 lan/giay)      : het bo nho heap khi tao task / hang doi          */
static void Fault_Blink(uint32_t n)
{
    volatile uint32_t i;
    while (1)
    {
        Led_Toggle();
        for (i = 0; i < n; i++)
        {
        }
    }
}

void HardFault_Handler(void)
{
    Fault_Blink(1200000);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    Led_On();
    while (1)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    Fault_Blink(6000000);
}

/* ===================== MAIN ===================== */
int main(void)
{
    // SystemInit() da duoc file startup goi truoc main() nen KHONG goi lai o day
    // (ban cu goi 2 lan va quen i2c_init nen man hinh khong chay)

    // 1. Khoi tao ngoai vi (truoc khi chay RTOS)
    i2c_init(1, 0); // I2C1: PB6 = SCL, PB7 = SDA (100kHz)
    Keypad_Init();
    Led_Init();
    Servo_Init();                // PB1 = TIM3_CH4, PWM 50Hz
    Servo_Write(SERVO_GOC_DONG); // dam bao cua dong luc khoi dong
    Spi1_Init();
    Store_Load(); // doc danh sach the + mat khau tu Flash

    // 2. Tao cac hang doi
    xEventQueue = xQueueCreate(8, sizeof(Event_t));
    xLcdQueue = xQueueCreate(6, sizeof(LcdMsg_t));

    if (xEventQueue == NULL || xLcdQueue == NULL)
    {
        Led_On(); // het bo nho heap -> sang LED bao loi va dung lai
        while (1)
        {
        }
    }

    // 3. Tao cac task (ten, do sau stack tinh theo word 4 byte, uu tien)
    xTaskCreate(Task_Access, "access", 320, NULL, 3, NULL);
    xTaskCreate(Task_Keypad, "keypad", 192, NULL, 2, NULL);
    xTaskCreate(Task_RFID, "rfid", 256, NULL, 2, NULL);
    xTaskCreate(Task_Display, "disp", 256, NULL, 1, NULL);

    // 4. Bat dau he dieu hanh
    vTaskStartScheduler();

    while (1)
    {
    }
}
