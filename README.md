```c
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
```