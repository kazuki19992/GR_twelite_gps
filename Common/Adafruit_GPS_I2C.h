#ifndef  ADAFRUIT_GPS_I2C_H_INCLUDED
#define  ADAFRUIT_GPS_I2C_H_INCLUDED

#define NMEA_MAX_WP_ID                                                         \
  20 ///< maximum length of a waypoint ID name, including terminating 0
#define NMEA_MAX_SENTENCE_ID                                                   \
  20 ///< maximum length of a sentence ID name, including terminating 0
#define NMEA_MAX_SOURCE_ID                                                     \
  3 ///< maximum length of a source ID name, including terminating 0

/// type for resulting code from running check()
typedef enum {
  NMEA_BAD = 0, ///< passed none of the checks
  NMEA_HAS_DOLLAR =
  1, ///< has a dollar sign or exclamation mark in the first position
  NMEA_HAS_CHECKSUM = 2,   ///< has a valid checksum at the end
  NMEA_HAS_NAME = 4,       ///< there is a token after the $ followed by a comma
  NMEA_HAS_SOURCE = 10,    ///< has a recognized source ID
  NMEA_HAS_SENTENCE = 20,  ///< has a recognized sentence ID
  NMEA_HAS_SENTENCE_P = 40 ///< has a recognized parseable sentence ID
} nmea_check_t;

#pragma pack(2)
typedef struct{
    short hh;
    short mm;
    short ss;
}Date; //6byte;

typedef struct{
    short dd;
    short mm;
    short mmmm;
}Degree; //6byte;

typedef struct{ 
    char name[13];
    char NorS; // 北緯 or 南緯
    char EorW; // 東経 or 西経
    char quality; // GGA:{0, 1, 2} RMC:{V, A}
    Date date;     //
    Degree latitude; // 緯度 
    Degree longitude; // 経度
}GpsData;
#pragma pack()

// 送信用データ作成する
	#pragma pack(1)
	typedef struct
	{
		GpsData gps;
		char ttl;
	}SendData;

	#pragma pack()

int32 gps_read(GpsData* gps);
int gps_parse(char* nmea, GpsData* gps);

void showGPS(GpsData* gps);

#endif // ADAFRUIT_GPS_I2C_H_INCLUDED