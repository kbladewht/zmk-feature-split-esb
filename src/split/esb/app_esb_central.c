

#include "app_esb.h"
#include "timeslot.h"
// #include <cstdint>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <esb.h>

#include <zmk/events/activity_state_changed.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_SPLIT_ESB_LOG_LEVEL);

 uint8_t rf_tx_freq_index = 0; // 当剝射频通靓��

 #pragma pack(4)

__attribute__((aligned(4))) uint8_t rf_freq_table[5] = { 4, 42, 77, 33, 26 };

#pragma pack()

static void event_handler_central(struct esb_evt const *event) {
    app_esb_event_t m_event;
    switch (event->evt_id) {
    case ESB_EVENT_TX_SUCCESS:
        // LOG_DBG("TX SUCCESS, tx_attempts: %d", event->tx_attempts);
         LOG_INF("ESB_EVENT_TX_SUCCESS");
        // Clear retry entry for the message that succeeded
        break;
    case ESB_EVENT_TX_FAILED:
        LOG_INF("TXss FAILED");
         rf_tx_freq_index++;

        if (rf_tx_freq_index >= 5) {
            rf_tx_freq_index = 0;
        }
        esb_set_rf_channel(rf_freq_table[rf_tx_freq_index]);
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

uint8_t inited = 0;
int esb_initialize_tx(void) {
    
    if (inited) {
        return 0;
    }

    inited = 1;
    
    int err;
    struct esb_config config = ESB_DEFAULT_CONFIG;

    config.protocol = ESB_PROTOCOL_ESB_DPL;
    config.retransmit_delay = 250;
    config.retransmit_count = 3;
    config.bitrate = ESB_BITRATE_1MBPS;
    // config.use_fast_ramp_up = true;
    config.payload_length = 0; // 后加
    config.event_handler = event_handler_central;
    config.mode = ESB_MODE_PTX;
    // config.tx_mode = ESB_TXMODE_MANUAL_START;
    // config.selective_auto_ack = true;

    err = esb_init(&config);

    if (err) {
        return err;
    }
    // esb_enable_pipes(0x03); // 同时开启 Pipe 0 (0x01) 和 Pipe 1 (0x02)

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
extern  bool m_active;
uint8_t add_to_queue(struct esb_payload tx_payloadn) {


    tx_payloadn.noack = false;
    tx_payloadn.pipe = 0;

    extern struct k_msgq m_msgq_tx_payloads;

    int ret = 0;

    ret = k_msgq_put(&m_msgq_tx_payloads, &tx_payloadn, K_NO_WAIT);

    if (ret == 0) {
       extern  uint32_t m_msgq_full_last_time;
        m_msgq_full_last_time = 0;
    } else if (ret == -ENOMSG) {
       
    } else {
        LOG_DBG("Failed to queue esb tx_payload_q (%d)", ret);
    }
    if (m_active) {
        pull_packet_from_tx_msgq();
    }

    return 1;
}

extern  uint16_t m_current_tx_msg_id;
int app_esb_resume_tx_central(void) {

    // LOG_INF("app_esb_resume_tx_central");
    // 开启TX
    int err = esb_initialize_tx();
    if (err) {
        LOG_ERR("esb_initialize_tx failed: %d", err);
        return err;
    }
    m_active = true;
    // clear_retry_table();
    m_current_tx_msg_id = 0;
    pull_packet_from_tx_msgq();
    return err;
}



int zmk_split_esb_send_ct(uint8_t *report) {
     esb_initialize_tx() ;
    int ret = 0;
    struct esb_payload tx_payload;
    tx_payload.pipe = 0;
    tx_payload.noack = true;
    memcpy(tx_payload.data, &report[1], 8);
    tx_payload.length = 8;

    for (int i = 0; i < tx_payload.length; i++) {
        printk("%02X ", tx_payload.data[i]);
    }
    printk("\n");

    ret = esb_write_payload(&tx_payload);
    ret = esb_start_tx();

    return 1;
}