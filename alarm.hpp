#pragma once
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace hexdesk {
enum class AlarmRepeat { Once, Daily, Weekly };
struct Alarm {
    AlarmRepeat repeat=AlarmRepeat::Once;
    int minute=540,weekdays=127; // Sunday is bit 0.
    std::string date;
    bool enabled=false;
    std::int64_t lastFired=0,nextDue=0; // nextDue is a runtime cache, never serialized.
};
inline std::tm LocalTime(std::int64_t value) {
    auto clock=static_cast<std::time_t>(value);std::tm result{};
#ifdef _WIN32
    localtime_s(&result,&clock);
#else
    localtime_r(&clock,&result);
#endif
    return result;
}
inline std::string DateText(const std::tm& date) {
    std::ostringstream out;out<<std::put_time(&date,"%Y-%m-%d");return out.str();
}
inline std::string AlarmTimeText(int minute) {
    std::ostringstream out;out<<std::setfill('0')<<std::setw(2)<<minute/60<<':'<<std::setw(2)<<minute%60;return out.str();
}
inline bool ParseAlarmTime(const std::string& value,int& minute) {
    if(value.size()!=5 || value[2]!=':')return false;
    for(int i:{0,1,3,4})if(value[i]<'0'||value[i]>'9')return false;
    int hour=(value[0]-'0')*10+value[1]-'0',part=(value[3]-'0')*10+value[4]-'0';
    if(hour>23||part>59)return false;minute=hour*60+part;return true;
}
inline bool ParseAlarmDate(const std::string& value,std::tm& date) {
    if(value.size()!=10 || value[4]!='-' || value[7]!='-')return false;
    for(int i:{0,1,2,3,5,6,8,9})if(value[i]<'0'||value[i]>'9')return false;
    int year=std::stoi(value.substr(0,4)),month=std::stoi(value.substr(5,2)),day=std::stoi(value.substr(8,2));
    if(year<1970||year>2099||month<1||month>12)return false;
    constexpr int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int limit=days[month-1]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
    if(day<1||day>limit)return false;
    date={};date.tm_year=year-1900;date.tm_mon=month-1;date.tm_mday=day;date.tm_isdst=-1;return true;
}
inline bool ValidAlarm(const Alarm& alarm) {
    int repeat=static_cast<int>(alarm.repeat);std::tm date{};
    return repeat>=0&&repeat<=2&&alarm.minute>=0&&alarm.minute<1440&&alarm.weekdays>=0&&alarm.weekdays<=127&&
        (alarm.repeat!=AlarmRepeat::Weekly||alarm.weekdays!=0)&&alarm.lastFired>=0&&alarm.lastFired<=4102444799LL&&
        (alarm.date.empty()||ParseAlarmDate(alarm.date,date))&&
        (!alarm.enabled||alarm.repeat!=AlarmRepeat::Once||!alarm.date.empty());
}
inline std::int64_t LocalOccurrence(std::tm day,int minute) {
    day.tm_hour=minute/60;day.tm_min=minute%60;day.tm_sec=0;day.tm_isdst=-1;
    auto expected=day;auto result=std::mktime(&day);
    // Reject a nonexistent wall time during a DST forward jump, rather than silently shifting it.
    if(result<0||day.tm_year!=expected.tm_year||day.tm_mon!=expected.tm_mon||day.tm_mday!=expected.tm_mday||
        day.tm_hour!=expected.tm_hour||day.tm_min!=expected.tm_min)return 0;
    return static_cast<std::int64_t>(result);
}
inline std::int64_t NextAlarm(const Alarm& alarm,std::int64_t now) {
    if(!ValidAlarm(alarm))return 0;
    auto after=std::max(now,alarm.lastFired);
    if(alarm.repeat==AlarmRepeat::Once) {
        std::tm date{};if(!ParseAlarmDate(alarm.date,date))return 0;
        auto at=LocalOccurrence(date,alarm.minute);return at>after?at:0;
    }
    auto day=LocalTime(after);day.tm_hour=12;day.tm_min=day.tm_sec=0;
    // Calendar days, not 86400-second increments: local dates can have 23 or 25 hours.
    for(int offset=0;offset<15;++offset) {
        auto candidate=day;candidate.tm_mday+=offset;candidate.tm_isdst=-1;
        if(std::mktime(&candidate)<0 || candidate.tm_year>199)return 0;
        if(alarm.repeat==AlarmRepeat::Weekly && !(alarm.weekdays&(1<<candidate.tm_wday)))continue;
        if(alarm.lastFired && DateText(candidate)<=DateText(LocalTime(alarm.lastFired)))continue;
        auto at=LocalOccurrence(candidate,alarm.minute);if(at>after)return at;
    }
    return 0;
}
inline void ArmAlarm(Alarm& alarm,std::int64_t now) {
    alarm.nextDue=alarm.enabled?NextAlarm(alarm,now):0;
    if(alarm.enabled&&!alarm.nextDue)alarm.enabled=false;
}
inline bool AdvanceAlarm(Alarm& alarm,std::int64_t now) {
    if(!alarm.enabled||!alarm.nextDue||now<alarm.nextDue)return false;
    alarm.lastFired=std::max(alarm.lastFired,alarm.nextDue);
    // Consume only the latest due slot after sleep/forward jumps, without a burst of
    // old alerts. Retain its calendar date to avoid ringing twice in a DST fold.
    if(alarm.repeat!=AlarmRepeat::Once) {
        auto day=LocalTime(now);day.tm_hour=12;day.tm_min=day.tm_sec=0;
        for(int offset=0;offset<8;++offset) {
            auto candidate=day;candidate.tm_mday-=offset;candidate.tm_isdst=-1;
            if(std::mktime(&candidate)<0)continue;
            if(alarm.repeat==AlarmRepeat::Weekly&&!(alarm.weekdays&(1<<candidate.tm_wday)))continue;
            auto at=LocalOccurrence(candidate,alarm.minute);
            if(at>0&&at<=now){alarm.lastFired=std::max(alarm.lastFired,at);break;}
        }
    }
    if(alarm.repeat==AlarmRepeat::Once){alarm.enabled=false;alarm.nextDue=0;}
    else ArmAlarm(alarm,now);
    return true;
}
} // namespace hexdesk
