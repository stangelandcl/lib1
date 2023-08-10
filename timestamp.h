#ifndef TIMESTAMP_H
#define TIMESTAMP_H

static int64_t timestamp_date(int year, int month, int day) {
    static const int month_days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap_years, seconds, is_leap_year, is_leap_day;    
    const int seconds_per_day = 86400;
    
    if((unsigned)year < 1970) return 0;
    if(month < 1 || month > 12) return 0;    
    
    leap_years = (year - 1968) / 4;
    is_leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if(is_leap_year && month < 3) --leap_years;
    
    is_leap_day = month == 2 && day == 29 && is_leap_year;
    
    seconds = (year - 1970) * 365 * seconds_per_day;
    seconds += leap_years * seconds_per_day;  
    seconds += (month_days[--month] + day - 1) * seconds_per_day;
    return seconds;
}

static int64_t timestamp_time(
	int year, int month, int day, int hour, 
	int minute, int second) {
    int64_t result = timestamp_date(year, month, day);
   
    if((unsigned)hour >= 24) return 0;
    if((unsigned)minute >= 60) return 0;
    if((unsigned)second >= 60) return 0;
    
    return result + hour * 60 * 60 + minute * 60 + second;
}

static void timestamp_explode(int64_t timestamp, 
	int *year, int *month, int *day,
	int *hour, int *minute, int *second) {
	int64_t years, months, days, hours, minutes, seconds;
	days = timestamp / 86400;
	seconds = timestamp - days * 86400;
	hours = seconds / 3600;
	seconds -= hours * 3600;
	minutes = seconds / 60;
	seconds -= minutes * 60;

	years = days / 365;
	leap_years = years / 4;

}

/* YYYY-MM-DD */
static void timestamp2date(char *buf, size_t nbuf, int64_t timestamp) {

}
/* YYYY-MM-DD HH:mm:ss */
static void timestamp2time(char *buf, size_t nbuf, int64_t timestamp) {

}


#endif

