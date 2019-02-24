/*
 * File:   cli.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     Implement a CLI to pass commands to the device
 */
#include "cli.h"

/**
 * CLIInit()
 *     Description:
 *         Initialize our CLI object
 *     Params:
 *         UART_t *uart - A pointer to the UART module object
 *         BC127_t *bt - A pointer to the BC127 object
 *         IBus_t *bt - A pointer to the IBus object
 *     Returns:
 *         void
 */
CLI_t CLIInit(UART_t *uart, BC127_t *bt, IBus_t *ibus)
{
    CLI_t cli;
    cli.uart = uart;
    cli.bt = bt;
    cli.ibus = ibus;
    cli.lastChar = 0;
    return cli;
}

/**
 * CLIProcess()
 *     Description:
 *         Read the RX queue and process the messages into meaningful data
 *     Params:
 *         UART_t *uart - A pointer to the UART module object
 *     Returns:
 *         void
 */
void CLIProcess(CLI_t *cli)
{
    while (cli->lastChar != cli->uart->rxQueue.writeCursor) {
        UARTSendChar(cli->uart, CharQueueGet(&cli->uart->rxQueue, cli->lastChar));
        if (cli->lastChar >= 255) {
            cli->lastChar = 0;
        } else {
            cli->lastChar++;
        }
    }
    uint8_t messageLength = CharQueueSeek(&cli->uart->rxQueue, CLI_MSG_END_CHAR);
    if (messageLength > 0) {
        // Send a newline to keep the CLI pretty
        UARTSendChar(cli->uart, 0x0A);
        char msg[messageLength];
        uint8_t i;
        uint8_t delimCount = 1;
        for (i = 0; i < messageLength; i++) {
            char c = CharQueueNext(&cli->uart->rxQueue);
            if (c == CLI_MSG_DELIMETER) {
                delimCount++;
            }
            if (c != CLI_MSG_END_CHAR) {
                msg[i] = c;
            } else {
                // 0x0D delimits messages, so we change it to a null
                // terminator instead
                msg[i] = '\0';
            }
        }
        // Copy the message, since strtok adds a null terminator after the first
        // occurrence of the delimiter, it will not cause issues with string
        // functions
        char tmpMsg[messageLength];
        strcpy(tmpMsg, msg);
        char *msgBuf[delimCount];
        char *p = strtok(tmpMsg, " ");
        i = 0;
        while (p != NULL) {
            msgBuf[i++] = p;
            p = strtok(NULL, " ");
        }

        if (strcmp(msgBuf[0], "BOOTLOADER") == 0) {
            LogRaw("Rebooting into bootloader\r\n");
            ConfigSetBootloaderMode(0x01);
            // Nop until the message reaches the terminal
            uint16_t i;
            for (i = 0; i < 256; i++) {
                Nop();
            }
            __asm__ volatile ("reset");
        } else if (strcmp(msgBuf[0], "BTRESET") == 0) {
            BC127CommandReset(cli->bt);
        } else if (strcmp(msgBuf[0], "BTRESETPDL") == 0) {
            BC127CommandUnpair(cli->bt);
        } else if (strcmp(msgBuf[0], "GET") == 0) {
            if (strcmp(msgBuf[1], "IBUSD") == 0) {
                IBusCommandGTGetDiagnostics(cli->ibus);
                IBusCommandRADGetDiagnostics(cli->ibus);
            } else if (strcmp(msgBuf[1], "UI") == 0) {
                unsigned char uiMode = ConfigGetUIMode();
                if (uiMode == IBus_UI_CD53) {
                    LogRaw("UI Mode: CD53\r\n");
                } else if (uiMode == IBus_UI_BMBT) {
                    LogRaw("UI Mode: BMBT\r\n");
                } else {
                    LogRaw("UI Mode: Not set or Invalid\r\n");
                }
            }
        } else if (strcmp(msgBuf[0], "REBOOT") == 0) {
            __asm__ volatile ("reset");
        } else if (strcmp(msgBuf[0], "SET") == 0) {
            if (strcmp(msgBuf[1], "AUDIO") == 0) {
                if (strcmp(msgBuf[2], "ANALOG") == 0) {
                    BC127CommandSetAudioDigital(
                        cli->bt,
                        BC127_AUDIO_I2S,
                        "44100",
                        "64",
                        "100800"
                    );
                } else if (strcmp(msgBuf[2], "DIGITAL") == 0) {
                    BC127CommandSetAudioDigital(
                        cli->bt,
                        BC127_AUDIO_SPDIF,
                        "48000",
                        "0",
                        "000000"
                    );
                }
            } else if (strcmp(msgBuf[1], "UI") == 0) {
                if (strcmp(msgBuf[2], "1") == 0) {
                    ConfigSetUIMode(IBus_UI_CD53);
                    LogRaw("UI Mode: CD53\r\n");
                } else if (strcmp(msgBuf[2], "2") == 0) {
                    ConfigSetUIMode(IBus_UI_BMBT);
                    LogRaw("UI Mode: BMBT\r\n");
                } else {
                    LogError("Invalid UI Mode specified");
                }
            } else if(strcmp(msgBuf[1], "IGN") == 0) {
                if (strcmp(msgBuf[2], "0") == 0) {
                    IBusCommandIgnitionStatus(cli->ibus, 0x00);
                } else if (strcmp(msgBuf[2], "1") == 0) {
                    IBusCommandIgnitionStatus(cli->ibus, 0x01);
                }
            } else if (strcmp(msgBuf[1], "LOG") == 0) {
                unsigned char system = 0xFF;
                unsigned char value = 0xFF;
                // Get the system
                if (strcmp(msgBuf[2], "BT") == 0) {
                    system = CONFIG_DEVICE_LOG_BT;
                } else if (strcmp(msgBuf[2], "IBUS") == 0) {
                    system = CONFIG_DEVICE_LOG_IBUS;
                } else if (strcmp(msgBuf[2], "SYS") == 0) {
                    system = CONFIG_DEVICE_LOG_SYSTEM;
                } else if (strcmp(msgBuf[2], "UI") == 0) {
                    system = CONFIG_DEVICE_LOG_UI;
                }
                // Get the value
                if (strcmp(msgBuf[3], "0") == 0) {
                    value = 0;
                } else if (strcmp(msgBuf[3], "1") == 0) {
                    value = 1;
                }
                if (system != 0xFF && value != 0xFF) {
                    ConfigSetLog(system, value);
                    LogRaw("Ok\r\n");
                } else {
                    LogRaw("Invalid Parameters for SET LOG\r\n");
                }
            }
        } else if (strcmp(msgBuf[0], "HELP") == 0 || strlen(msgBuf[0]) == 0) {
            LogRaw("BlueBus Firmware version: 1.0.3\r\n");
            LogRaw("Available Commands:\r\n");
            LogRaw("    BOOTLOADER - Reboot into the bootloader immediately\r\n");
            LogRaw("    BTREBOOT - Reboot the BC127\r\n");
            LogRaw("    BTRESETPDL - Unpair all devices from the BC127\r\n");
            LogRaw("    GET UI - Get the current UI Mode\r\n");
            LogRaw("    REBOOT - Reboot the device\r\n");
            LogRaw("    SET AUDIO x - Set the audio output where x is ANALOG ");
            LogRaw("    or DIGITAL. DIGITAL is the coax output.\r\n");
            LogRaw("    SET IGN x - Send the ignition status message [DEBUG]\r\n");
            LogRaw("    SET LOG x y - Change logging for x (BT, IBUS, SYS, UI)");
            LogRaw("to y (1 = On, 0 = Off)\r\n");
            LogRaw("    SET UI x - Set the UI to x, ");
            LogRaw("where 1 is CD53 and 2 is BMBT\r\n");
        } else {
            LogError("Command Unknown. Try HELP");
        }
    }
}