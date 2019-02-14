/*
 * File:   eeprom.c
 * Author: Ted Salmon <tass2001@gmail.com>
 * Description:
 *     EEPROM mechanisms
 */
#include "eeprom.h"

/**
 * EEPROMInit()
 *     Description:
 *         Initialize the EEPROM on SPI1 with the predefined values
 *     Params:
 *         void
 *     Returns:
 *         void
 */
void EEPROMInit()
{
    EEPROM_CS_IO_MODE = 0;
    EEPROM_CS_PIN = 1;
    SPI1CON1L = 0;
    SPI1STATLbits.SPIRBF = 0;
    // Unlock the programmable pin register
    __builtin_write_OSCCONL(OSCCON & 0xBF);
    // Data Input
    _SDI1R = EEPROM_SDI_PIN;
    // Set the SCK Output
    EEPROM_SCK_PIN = EEPROM_SPI_SCK_MODE;
    // Set the SDO Output
    EEPROM_SDO_PIN = EEPROM_SPI_SDO_MODE;
    // Lock the programmable pin register
    __builtin_write_OSCCONL(OSCCON & 0x40);
    SPI1BRGL = EEPROM_BRG;
    SPI1STATLbits.SPIROV = 0;
    // Enable Module | Set CKE to active -> idle | Master Enable
    SPI1CON1L = 0b1000000100100000;
}

/**
 * EEPROMSend()
 *     Description:
 *         Write to the SPI buffer and return the received value
 *     Params:
 *         char data - The data to transfer to the EEPROM
 *     Returns:
 *         unsigned char - The 8-bit byte returned from the EEPROM
 */
static unsigned char EEPROMSend(char data)
{
    SPI1BUFL = data;
    while (!SPI1STATLbits.SPIRBF);
    return SPI1BUFL;
}

/**
 * EEPROMEnableWrite()
 *     Description:
 *         Perform the necessary actions to set up the EEPROM for writing
 *     Params:
 *         void
 *     Returns:
 *         void
 */
static void EEPROMEnableWrite()
{
    // Wait until EEPROM is not busy
    EEPROMIsReady();
    EEPROM_CS_PIN = 0;
    EEPROMSend(EEPROM_COMMAND_WREN);
    EEPROM_CS_PIN = 1;
    EEPROM_CS_PIN = 0;
}

/**
 * EEPROMDestroy()
 *     Description:
 *         Destory the EEPROM configuration so that the application can use the
 *         SPI module again
 *     Params:
 *         void
 *     Returns:
 *         void
 */
void EEPROMDestroy()
{
    EEPROM_CS_IO_MODE = 0;
    EEPROM_CS_PIN = 1;
    SPI1CON1L = 0;
    SPI1STATLbits.SPIRBF = 0;
    // Unlock the programmable pin register
    __builtin_write_OSCCONL(OSCCON & 0xBF);
    // Data Input
    _SDI1R = 0;
    // Reset the SCK Output
    EEPROM_SCK_PIN = 0;
    // Reset the SDO Output
    EEPROM_SDO_PIN = 0;
    // Lock the programmable pin register
    __builtin_write_OSCCONL(OSCCON & 0x40);
    SPI1BRGL = 0;
    SPI1STATLbits.SPIROV = 0;
    // Disable Module
    SPI1CON1L = 0;
}

/**
 * EEPROMIsReady()
 *     Description:
 *         Check with the EEPROM to see if it's ready to be written to. If it
 *         is not, this function blocks until it is ready (status 0x00).
 *     Params:
 *         void
 *     Returns:
 *         void
 */
void EEPROMIsReady()
{
    char status = EEPROM_STATUS_BUSY;
    while (status & EEPROM_STATUS_BUSY) {
        EEPROM_CS_PIN = 0;
        EEPROMSend(EEPROM_COMMAND_RDSR);
        status = EEPROMSend(EEPROM_COMMAND_GET);
        EEPROM_CS_PIN = 1;
    }
}

/**
 * EEPROMReadByte()
 *     Description:
 *         Read a byte from the EEPROM at the given address and return it
 *     Params:
 *         unsigned char address - The memory address of the byte to retrieve
 *     Returns:
 *         unsigned char - The byte at the given address
 */
unsigned char EEPROMReadByte(unsigned char address)
{
    EEPROMIsReady();
    EEPROM_CS_PIN = 0;
    EEPROMSend(EEPROM_COMMAND_READ);
    // Address must be 16-bits so we transfer it in two 8-bit sessions
    EEPROMSend(address >> 8);
    EEPROMSend(address);
    // Cast return of EEPROM send to an 8-bit byte, since the returned register
    // is always 16 bits
    unsigned char data = (unsigned char) ((uint8_t) EEPROMSend(EEPROM_COMMAND_GET));
    EEPROM_CS_PIN = 1; // Release EEPROM
    return data;
}

/**
 * EEPROMWriteByte()
 *     Description:
 *         Check with the EEPROM to see if it is ready to be written to. If it
 *         is not, this function blocks until it is ready (status 0x00).
 *     Params:
 *         unsigned char address - The memory address of the byte to retrieve
 *         unsigned char data - The 8-bit byte to write
 *     Returns:
 *         void
 */
void EEPROMWriteByte(unsigned char address, unsigned char data)
{
    EEPROMEnableWrite();
    EEPROMSend(EEPROM_COMMAND_WRITE);
    // Address must be 16-bits so we transfer it in two 8-bit sessions
    EEPROMSend(address >> 8);
    EEPROMSend(address);
    EEPROMSend(data);
    EEPROM_CS_PIN = 1;
}
