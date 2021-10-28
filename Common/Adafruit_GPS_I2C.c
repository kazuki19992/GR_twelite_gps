
#include "AppHardwareApi.h"
#include "SMBus.h"
#include "sensor_driver.h"

#include "Adafruit_GPS_I2C.h"

#define GPS_MAX_I2C_TRANSFER  32
#define GPS_I2C_ADDR  0x10

#define GpsBufferSize 256
uint8 gpsBuffer[GpsBufferSize];
int gps_buffer_index = 0;

#include "ToCoNet.h"
extern tsFILE sSerStream;

#define false 0
#define true 1

int32 gps_read(GpsData* gps)
{
    int i,j;
    static uint8 last_char = 0;;
	bool_t bOk = TRUE;
    uint8 data[GPS_MAX_I2C_TRANSFER];

    bOk &= bSMBusSequentialRead(GPS_I2C_ADDR, GPS_MAX_I2C_TRANSFER, data);
    if (bOk == FALSE) {
        return SENSOR_TAG_DATA_ERROR;
    }

    bOk = FALSE;
    for(i = 0 ; i < GPS_MAX_I2C_TRANSFER; i++){
        if( last_char != 0x0D && data[i] == 0x0A){ // !CR + LF
            continue;
        }
        gpsBuffer[gps_buffer_index++] = data[i];
        
        if(last_char == 0x0D && data[i] == 0x0A){ // CR + LF
            gpsBuffer[gps_buffer_index - 1] = '\0';
            gpsBuffer[gps_buffer_index - 2] = '\0';
            if(gps_parse(gpsBuffer, gps))
                bOk = true;
            gps_buffer_index = 0;
            continue;
        }
        last_char = data[i];
    }
    return bOk;
}

thisCheck = 0; ///< the results of the check on the current sentence
char thisSentence[NMEA_MAX_SENTENCE_ID] = {
  0 }; ///< the next three letters of the current sentence, e.g. GLL, RMC


const char *sentences_parsed[20] = {
      "GGA", "GLL", "GSA", "RMC", "DBT", "HDM", "HDT",
      "MDA", "MTW", "MWV", "RMB", "TXT", "VHW", "VLW",
      "VPW", "VWR", "WCV", "XTE", "ZZZ" }; ///< parseable sentence ids
const char *sentences_known[15] = {
      "APB", "DPT", "GSV", "HDG", "MWD", "ROT",
      "RPM", "RSA", "VDR", "VTG", "ZDA", "ZZZ" }; ///< known, but not parseable

// read a Hex value and return the decimal equivalent
unsigned char parseHex(char c) {
  if (c < '0')
    return 0;
  if (c <= '9')
    return c - '0';
  if (c < 'A')
    return 0;
  if (c <= 'F')
    return (c - 'A') + 10;
  // if (c > 'F')
  return 0;
}

inline int min(int a, int b){
    if(a < b ) return a;
    return b;
}


char* parseStr(char *buff, char *p, int n) {
  char *e = strchr(p, ',');
  int len = 0;
  if (e) {
    len = min(e - p, n - 1);
    strncpy(buff, p, len); // copy up to the comma
    buff[len] = 0;
  }
  else {
    e = strchr(p, '*');
    if (e) {
      len = min(e - p, n - 1);
      strncpy(buff, p, len); // or up to the *
      buff[e - p] = 0;
    }
    else {
      len = min((int)strlen(p), n - 1);
      strncpy(buff, p, len); // or to the end or max capacity
    }
  }
  return buff;
}

char* tokenOnList(char *token, char **list) {
    int i = 0; // index in the list
    while (strncmp(list[i], "ZZ", 2) && i < 1000) { // stop at terminator and don't crash without it
        // test for a match on the sentence name
        if (!strncmp((const char *)list[i], (const char *)token, strlen(list[i]))){
          return list[i];
        }
        i++;
    }
    return NULL; // couldn't find a match
}

int check(char *nmea) {
    thisCheck = 0; // new check
    *thisSentence = 0;
    
    char *ast = 0;
    if (*nmea != '$' && *nmea != '!')
        return false; // doesn't start with $ or !
    else
        thisCheck += NMEA_HAS_DOLLAR;
    // do checksum check -- first look if we even have one -- ignore all but last
    // *
    
    ast = nmea; // not strchr(nmea,'*'); for first *
    while (*ast)
        ast++; // go to the end
    while (*ast != '*' && ast > nmea)
        ast--; // then back to * if it's there
    
    if (*ast != '*')
        return false; // there is no asterisk
    else {
        char *p = nmea; // check checksum
        char *p1;
        uint16_t sum = parseHex(*(ast + 1)) * 16; // extract checksum
        sum += parseHex(*(ast + 2));
        
        for (p1 = p + 1; p1 < ast; p1++)
            sum ^= *p1;
        if (sum != 0)
            return false; // bad checksum :(
        else
            thisCheck += NMEA_HAS_CHECKSUM;
    }

    // extract source of variable length
    char *p = nmea + 3;
    char *src = tokenOnList(p, sentences_parsed);
  
    const char *snc = tokenOnList(p, sentences_parsed);
    if (snc) {
        strcpy(thisSentence, snc);
        thisCheck += NMEA_HAS_SENTENCE_P + NMEA_HAS_SENTENCE;
    }
    else { // check if known
        snc = tokenOnList(p, sentences_known);
        if (snc) {
            strcpy(thisSentence, snc);
            thisCheck += NMEA_HAS_SENTENCE;
            return false; // known but not parsed
        }
        else {
            parseStr(thisSentence, p, NMEA_MAX_SENTENCE_ID);
            return false; // unknown
        }
    }
    return true; // passed all the tests
}

char* parseDate(char* p, Date* date){
    char tmp[3];
    tmp[2] = '\0';

    memcpy(tmp, p, 2);
    date->hh = atoi(tmp);
    
    p += 2;
    memcpy(tmp, p, 2);
    date->mm = atoi(tmp);
    
    p += 2;
    memcpy(tmp, p, 2);
    date->ss = atoi(tmp);
    return p;
}

char* parseLatitude(char* p, Degree* deg){
    char tmp[5];
    tmp[2] = '\0';
    memcpy(tmp, p, 2);
    deg->dd = atoi(tmp);
    
    p += 2;
    memcpy(tmp, p, 2);
    deg->mm = atoi(tmp);
    
    p += 3;
    tmp[4] = '\0';
    memcpy(tmp, p, 4);
    deg->mmmm = atoi(tmp);
    
    return p;
}

char* parseLongitude(char* p, Degree* deg){
    char tmp[5];
    tmp[3] = '\0';
    memcpy(tmp, p, 3);
    deg->dd = atoi(tmp);
    
    p += 3;
    tmp[2] = '\0';
    memcpy(tmp, p, 2);
    deg->mm = atoi(tmp);
    
    p += 3;
    tmp[4] = '\0';
    memcpy(tmp, p, 4);
    deg->mmmm = atoi(tmp);
    
    return p;
}

bool gps_parse(char* nmea, GpsData* gps){
    if(!check(nmea)) return false;
    
    char *p = nmea;
    p = strchr(p, ',') + 1; // Skip to char after the next comma, then check.
    if (!strcmp(thisSentence, "GGA")) {
        p = parseDate(p, &gps->date);
        
        p = strchr(p, ',') + 1;
        if(*p == ',') return false;
        
        p = parseLatitude(p, &gps->latitude);
        
        p = strchr(p, ',') + 1;
        gps->NorS = *p;
        
        p = strchr(p, ',') + 1;
        p = parseLongitude(p, &gps->longitude);
        
        p = strchr(p, ',') + 1;
        gps->EorW = *p;
        
        p = strchr(p, ',') + 1;
        gps->quality = *p;
        return true;
    }else if(!strcmp(thisSentence, "RMC")){
        p = parseDate(p, &gps->date);
        
        p = strchr(p, ',') + 1;
        gps->quality = *(p);
        if(gps->quality == 'V') return false;
        
        p = strchr(p, ',') + 1;
        p = parseLatitude(p, &gps->latitude);
        
        p = strchr(p, ',') + 1;
        gps->NorS = *p;
        
        p = strchr(p, ',') + 1;
        p = parseLongitude(p, &gps->longitude);
        
        p = strchr(p, ',') + 1;
        gps->EorW = *p;
        
        return true;
    }
    return false;
}

void showGPS(GpsData* gps){
    //vfPrintf(&sSerStream, "Date: %d[h]%d[m]%d[s]\n", gps->date.hh, gps->date.mm, gps->date.ss);
    //vfPrintf(&sSerStream, "Lat: %d[h]%d.%d [%c]\n", gps->latitude.dd, gps->latitude.mm,gps->latitude.mmmm, gps->NorS);
    //vfPrintf(&sSerStream, "Lon: %d[h]%d.%d [%c]\n", gps->longitude.dd, gps->longitude.mm, gps->longitude.mmmm, gps->EorW);
    //vfPrintf(&sSerStream, "Quality: %c\n", gps->quality);
    vfPrintf(&sSerStream, "$POS,%s,%d:%d:%d,%d:%d:%d,%c,%d:%d:%d,%c,%c%c%c", 
        gps->name,
        gps->date.hh, gps->date.mm, gps->date.ss,
        gps->latitude.dd, gps->latitude.mm,gps->latitude.mmmm, gps->NorS,
        gps->longitude.dd, gps->longitude.mm, gps->longitude.mmmm, gps->EorW,
        gps->quality, 0x0D, 0x0A
    );
}
