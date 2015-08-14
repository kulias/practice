/// @file ccd.h
/// @brief CCD barcode scanner module header
/// @author Kenny Huang

#ifndef __CCD_H
#define __CCD_H

#define EMPLOYEE_ID_LENGTH 32

/// CCD barcode scanner mode
enum CcdMode {
    CCD_NONE        = 0,               ///< No mode has been set
    CCD_CHECK_IN    = 1,               ///< Employee check-in mode
    CCD_CHECK_OUT   = 2,               ///< Employee check-out mode
    CCD_BREAK       = 3,               ///< Employee out mode
    CCD_BREAK_BEGIN = 31,              ///< Start of take a break mode
    CCD_BREAK_END   = 32,              ///< End of take a break mode
    CCD_OUT         = 4,               ///< Employee back mode
    CCD_OUT_START   = 41,              ///< The start of going out mode
    CCD_OUT_END     = 42,              ///< The end of going out mode
    CCD_CONFIG      = 5,               ///< Admin configuration mode
    CCD_TEST        = 6,               ///< Scanner and webcam function test mode
    CCD_FOOD        = 10               ///< Food selection mode
};

/// CCD barcode scanner status
enum CcdStatus {
    CCD_READY = 0,  ///< The scanner is ready to scan
    CCD_SCANNED = 1 ///< The scanner is already scanned, and processing
};

/// @defgroup ccd CCD Barcode Scanner
/// @{

/// @brief Start a high priority thread to monitor connected port and read data
///        from scanner.
void StartCcdMonitor();

/// @brief Reset buffer for barcode values
void CcdReset();

/// @brief Load configuration of barcode scanner from database
void LoadBarcodeConfig();

/// @brief Set current CCD barcode scanner mode [@ref CcdMode]
void SetCcdMode(unsigned int set_mode);

/// @brief Get current CCD barcode scanner mode, there are three modes totally
/// @return @ref CcdMode
int GetCcdMode();

/// @brief Lock CCD barcode reader function, ignoring any input
void CcdLock();

/// @brief Unlock CCD barcode reader function
void CcdUnLock();

/// @}

#endif //__CCD_H 