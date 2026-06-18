

#include "app_esb.h"
#include "timeslot.h"
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <esb.h>

#include <zmk/events/activity_state_changed.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_SPLIT_ESB_LOG_LEVEL);

static void event_handler_central(struct esb_evt const *event) {
    app_esb_event_t m_event;
    switch (event->evt_id) {
    case ESB_EVENT_TX_SUCCESS:
        // LOG_DBG("TX SUCCESS, tx_attempts: %d", event->tx_attempts);
         LOG_INF("ESB_EVENT_TX_SUCCESS");
        // Clear retry entry for the message that succeeded

        break;
    case ESB_EVENT_TX_FAILED:
        LOG_INF("TX FAILED, tx_attempts: %d", event->tx_attempts);
        esb_flush_tx();
        esb_start_tx();
        break;
    case ESB_EVENT_RX_RECEIVED:
        LOG_INF("TX ESB_EVENT_RX_RECEIVED");
        break;
    }
}

uint8_t esb_base_addr_0_c[4] = {0xE7, 0xE7, 0xE7, 0xE7};
uint8_t esb_base_addr_1_c[4] = {0xC2, 0xC2, 0xC2, 0xC2};
uint8_t esb_addr_prefix_c[2] = {0xE7, 0xC2};

int esb_initialize_tx(void) {
    int err;
    struct esb_config config = ESB_DEFAULT_CONFIG;

    config.protocol = ESB_PROTOCOL_ESB_DPL;
    config.retransmit_delay = 250;
    config.retransmit_count = 3;
    config.bitrate = ESB_BITRATE_1MBPS;
    config.use_fast_ramp_up = true;
    config.payload_length = 0; // 后加
    config.event_handler = event_handler_central;
    config.mode = ESB_MODE_PTX;
    config.tx_mode = ESB_TXMODE_AUTO;
    // config.selective_auto_ack = true;

    err = esb_init(&config);

    if (err) {
        return err;
    }
    esb_enable_pipes(0x03); // 同时开启 Pipe 0 (0x01) 和 Pipe 1 (0x02)

    esb_set_rf_channel(26);
    err = esb_set_base_address_0(esb_base_addr_0_c);
    if (err) {
        return err;
    }

    err = esb_set_base_address_1(esb_base_addr_1_c);
    if (err) {
        return err;
    }

    err = esb_set_prefixes(esb_addr_prefix_c, ARRAY_SIZE(esb_addr_prefix_c));
    if (err) {
        return err;
    }

    // NVIC_SetPriority(RADIO_IRQn, 0);

    return 0;
}

uint8_t radio_send_keyboard2(uint8_t *report) {

    static struct esb_payload tx_payloadn;
    // memcpy(tx_payloadn.data, report, 8);

    tx_payloadn.length = 8;

    // 🔥 先清零已经做了，这里方便你后面改内容
    tx_payloadn.data[0] = 0;
    tx_payloadn.data[1] = 0;
    tx_payloadn.data[2] = 0;
    tx_payloadn.data[3] = 0;
    tx_payloadn.data[4] = 0;
    tx_payloadn.data[5] = 0;
    tx_payloadn.data[6] = 0x10;
    tx_payloadn.data[7] = 0;

    tx_payloadn.noack = true;
    tx_payloadn.pipe = 0;

    for (int i = 0; i < tx_payloadn.length; i++) {
        printk("%02X ", tx_payloadn.data[i]);
    }
    printk("\n");

   // 3. 写入 payload
    int err = esb_write_payload(&tx_payloadn);
    if (err) {
        LOG_ERR("esb_write_payload failed: %d", err);
        return err;
    }

    // 4. ⭐ 关键：真正启动无线电发送！
    // err = esb_start_tx();
    // if (err) {
    //     LOG_ERR("esb_start_tx failed: %d", err);
    // } else {
    //     LOG_INF("TX Started! Waiting for Dongle ACK...");
    // }

    // esb_flush_tx(); 

    return 1;
}
extern  bool m_active;
extern  uint16_t m_current_tx_msg_id;
int app_esb_resume_tx_central(void) {

    LOG_INF("app_esb_resume_tx_central");
    // 开启TX
    int err = esb_initialize_tx();
    if (err) {
        LOG_ERR("esb_initialize_tx failed: %d", err);
        return err;
    }
    m_active = true;
    clear_retry_table();
    m_current_tx_msg_id = 0;
    pull_packet_from_tx_msgq();
    return err;
}
