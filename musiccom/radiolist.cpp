#include "radiolist.h"

static const RadioStation kStations[] = {
    /* ── 中国大陆 ── */
    {"china", "CNR 中国之声",       "https://ngcdn001.cnr.cn/live/zgzs/index.m3u8", true},
    {"china", "CNR 经济之声",       "https://ngcdn002.cnr.cn/live/jjzs/index.m3u8", true},
    {"china", "CNR 音乐之声",       "https://ngcdn001.cnr.cn/live/yyzs/index.m3u8", true},
    {"china", "CNR 中华之声",       "http://ngcdn004.cnr.cn/live/zhzs/index.m3u8",  true},
    {"china", "CNR 文艺之声",       "http://ngcdn004.cnr.cn/live/wyzs/index.m3u8",  true},
    {"china", "CNR 经典音乐广播",   "http://satellitepull.cnr.cn/live/wxdszs/playlist.m3u8", true},
    {"china", "CNR 台海之声",       "http://ngcdn004.cnr.cn/live/dszs/index.m3u8",  true},
    {"china", "CRI 华语环球广播",   "http://sk.cri.cn/hyhq.m3u8",  true},
    {"china", "CRI 环球资讯",       "http://sk.cri.cn/905.m3u8",   true},
    {"china", "CRI 世界华声",       "http://sk.cri.cn/hxfh.m3u8",  true},
    {"china", "CRI China Plus",     "https://sk.cri.cn/am846.m3u8", true},
    {"china", "CRI 南海之声",       "https://sk.cri.cn/nhzs.m3u8", true},
    {"china", "中国之声(蜻蜓备源)", "https://lhttp.qtfm.cn/live/15318317/64k.mp3", false},
    {"china", "上海动感101",        "https://lhttp.qingting.fm/live/274/64k.mp3",  false},
    {"china", "上海经典音乐广播",   "http://lhttp.qingting.fm/live/267/64k.mp3",   false},
    {"china", "上海经典金曲 FM103.7","https://lhttp-hw.qtfm.cn/live/273/64k.mp3",  false},
    {"china", "上海新闻广播",       "http://lhttp.qingting.fm/live/270/64k.mp3",   false},
    {"china", "北京音乐广播",       "https://lhttp.qtfm.cn/live/332/64k.mp3",      false},
    {"china", "北京交通广播",       "https://lhttp.qingting.fm/live/336/64k.mp3",  false},
    {"china", "北京新闻广播",       "https://lhttp.qtfm.cn/live/339/64k.mp3",      false},
    {"china", "第一财经",           "http://lhttp.qingting.fm/live/276/64k.mp3",   false},
    {"china", "广东音乐之声",       "https://lhttp.qtfm.cn/live/1260/64k.mp3",     false},
    {"china", "广东交通之声",       "https://lhttp.qtfm.cn/live/1262/64k.mp3",     false},
    {"china", "广东城市之声",       "https://lhttp.qtfm.cn/live/469/64k.mp3",      false},
    {"china", "广东股市广播",       "https://lhttp.qtfm.cn/live/4847/64k.mp3",     false},
    {"china", "江苏经典流行音乐",   "https://lhttp.qtfm.cn/live/4938/64k.mp3",     false},
    {"china", "安徽小说评书广播",   "https://lhttp.qtfm.cn/live/1951/64k.mp3",     false},
    {"china", "四川新闻广播",       "https://lhttp.qtfm.cn/live/4906/64k.mp3",     false},
    {"china", "郑州新闻广播",       "http://lhttp.qingting.fm/live/1220/64k.mp3",  false},
    {"china", "怀集音乐之声",       "https://lhttp.qingting.fm/live/4804/64k.mp3", false},
    {"china", "江苏新闻广播",       "http://lzlive.vojs.cn/5Fo8mMX/92/live.m3u8",  true},
    {"china", "江苏交通广播网",     "http://lzlive.vojs.cn/4TaHTeL/92/live.m3u8",  true},
    {"china", "江苏故事广播",       "http://lzlive.vojs.cn/rWjyus9/92/live.m3u8",  true},
    {"china", "河南戏曲广播",       "https://stream.hndt.com/live/yule/playlist.m3u8",  true},
    {"china", "河南新闻广播",       "https://stream.hndt.com/live/xinwen/playlist.m3u8", true},
    {"china", "山东经典音乐广播",   "http://audiolive302.iqilu.com/sdradioShenghuo/sdradio04/playlist.m3u8", true},
    {"china", "凤凰优悦广播",       "http://playtv-live.ifeng.com/live/06OLEGEGM4G_audio.m3u8", true},
    {"china", "怀旧音乐广播",       "https://live.ximalaya.com/radio-first-page-app/live/2629/64.m3u8", true},
    {"china", "雨声轻音乐",         "https://stream.zeno.fm/689zc32y4x8uv", false},
    {"china", "德云社相声合集",     "https://stream.zeno.fm/yqawwmweq8mtv", false},
    {"china", "REPLAY NEWS 中文",   "https://replaynewszh.ice.infomaniak.ch/replaynewszh-128.mp3", false},
    /* ── 港台 ── */
    {"hongkong", "RTHK Radio 1",       "http://stm.rthk.hk/radio1", false},
    {"hongkong", "RTHK Radio 2",       "http://stm.rthk.hk/radio2", false},
    {"hongkong", "AsiaFM 亚洲音乐台",  "https://live.xmcdn.com/live/764/64.m3u8", true},
    {"hongkong", "AsiaFM HD 音乐台",   "http://asiafm.hk:8000/asiahd", false},
    {"hongkong", "台北国际社区 ICRT",  "https://www.icrt.com.tw/icrt_channel.php", false},
    {"hongkong", "台湾教育广播电台",   "https://cast.ner.gov.tw/1", false},
    {"hongkong", "台湾警察广播电台",   "http://stream.pbs.gov.tw:1935/live/mp3:PBS/playlist.m3u8", true},
    {"hongkong", "高雄广播电台 FM94.3","https://stream.kbs.gov.tw/live/FM/playlist.m3u8", true},
    {"hongkong", "讲客广播电台",       "https://live.hakkaradio.org.tw/live/hakkafromTY/playlist.m3u8", true},
    {"hongkong", "CityFM 城市广播",    "http://fm901.cityfm.tw:8080/901.mp3", false},
    /* ── 亚洲其它 ── */
    {"asia", "Anison 动漫音乐台", "http://pool.anison.fm:9000/AniSonFM(320)", false},
    {"asia", "Big B Radio 亚洲流行","https://antares.dribbcast.com/proxy/apop?mp=/s", false},
    /* ── 国际 ── */
    {"international", "SomaFM Groove Salad",    "https://ice2.somafm.com/groovesalad-128-mp3", false},
    {"international", "SomaFM Secret Agent",    "https://ice6.somafm.com/secretagent-128-mp3", false},
    {"international", "SomaFM Underground 80s", "https://ice6.somafm.com/u80s-128-mp3", false},
    {"international", "SomaFM Space Station",   "https://ice6.somafm.com/spacestation-128-aac", false},
    {"international", "SomaFM Indie Pop Rocks", "https://ice6.somafm.com/indiepop-128-aac", false},
    {"international", "SomaFM Lush",            "https://ice5.somafm.com/lush-128-mp3", false},
    {"international", "SomaFM PopTron",         "https://ice2.somafm.com/poptron-128-mp3", false},
    {"international", "SomaFM Left Coast 70s",  "https://ice2.somafm.com/seventies-320-mp3", false},
    {"international", "SomaFM Metal Detector",  "https://ice2.somafm.com/metal-128-aac", false},
    {"international", "SomaFM Fluid",           "https://ice6.somafm.com/fluid-128-mp3", false},
    {"international", "SomaFM Heavyweight Reggae","https://ice2.somafm.com/reggae-256-mp3", false},
    {"international", "Radio Paradise Main Mix","http://stream-uk1.radioparadise.com/aac-320", false},
    {"international", "Radio Paradise Rock Mix","http://stream.radioparadise.com/rock-320", false},
    {"international", "Radio Paradise Mellow",  "http://stream.radioparadise.com/mellow-320", false},
    {"international", "Radio Paradise Main 128","http://stream.radioparadise.com/aac-128", false},
    {"international", ".977 Hitz",       "http://15313.live.streamtheworld.com/977_HITS_SC", false},
    {"international", ".977 80s",        "http://26433.live.streamtheworld.com/977_80_SC", false},
    {"international", ".977 The Mix",    "http://15113.live.streamtheworld.com:3690/977_MIX_SC", false},
    {"international", ".977 Smooth Jazz","http://15693.live.streamtheworld.com:3690/977_SMOOJAZZ_SC", false},
    {"international", ".977 Oldies",     "http://17573.live.streamtheworld.com:3690/977_OLDIES_SC", false},
    {"international", "181.FM Highway 181",     "http://listen.181fm.com/181-highway_128k.mp3", false},
    {"international", "181.FM Kickin' Country", "https://listen.181fm.com/181-kickincountry_128k.mp3", false},
    {"international", "181.FM True Blues",      "https://listen.181fm.com/181-blues_128k.mp3", false},
    {"international", "181.FM Super 70s",       "http://listen.181fm.com/181-70s_128k.mp3", false},
    {"international", "181.FM 80's HairBand",   "http://listen.181fm.com/181-hairband_128k.mp3", false},
    {"international", "Hot 108 JAMZ",    "http://sc.hot108.com:4000/", false},
    {"international", "Bassdrive",       "http://ice.bassdrive.net/stream", false},
    {"international", "Frisky",          "http://stream2.friskyradio.com/frisky_mp3_hi", false},
    {"international", "Chilltrax",       "http://server1.chilltrax.com:9000/", false},
    {"international", "Ambient Sleeping Pill",  "http://radio.stereoscenic.com/asp-h", false},
    {"international", "NPR 24 Hour",     "http://npr-ice.streamguys1.com/live.mp3", false},
    {"international", "Bloomberg WBBR",  "http://18853.live.streamtheworld.com:3690/WBBRAMAAC_SC", false},
    {"international", "FIP",             "https://icecast.radiofrance.fr/fip-midfi.mp3", false},
    {"international", "FIP Rock",        "https://icecast.radiofrance.fr/fiprock-midfi.mp3", false},
    {"international", "FIP Groove",      "https://icecast.radiofrance.fr/fipgroove-midfi.mp3", false},
    {"international", "MDR Sachsen",     "https://mdr-284350-ssl.mdr.de/mdr/284350/mdr_sachsen_live_high.mp3", false},
    {"international", "Bayern 2",        "https://dispatcher.rndfnk.com/br/br2/live/mp3/mid", false},
    {"international", "NTS Radio 1",     "https://stream-relay-geo.ntslive.net/stream", false},
    {"international", "NTS Radio 2",     "https://stream-relay-geo.ntslive.net/stream2", false},
    {"international", "Radio Caroline",  "https://sc6.radiocaroline.org.uk/", false},
    {"international", "Subcity Radio",   "https://radio.subcity.org/live.mp3", false},
    {"international", "Le Mellotron",    "https://listen.radioking.com/radio/121020/stream/277260", false},
    {"international", "Cashmere Radio",  "https://radio.cashmereradio.com/listen/cashmere_radio/radio.mp3", false},
    {"international", "Kiosk Radio",     "https://streaming.radio.co/s66v88nbfk/listen", false},
};

namespace radiolist {

static const char* const kRegions[] = { "china", "hongkong", "asia", "international" };

const char* const* regions(int& count)
{
    count = (int)(sizeof(kRegions) / sizeof(kRegions[0]));
    return kRegions;
}

QString regionTitleKey(const QString& regionKey)
{
    return QString("radio.region.") + regionKey;
}

const std::vector<RadioStation>& stations()
{
    static const std::vector<RadioStation> s(
        kStations, kStations + sizeof(kStations) / sizeof(kStations[0]));
    return s;
}

std::vector<RadioStation> byRegion(const QString& regionKey)
{
    std::vector<RadioStation> out;
    for (size_t i = 0; i < stations().size(); ++i)
        if (regionKey.isEmpty() || regionKey == stations()[i].region)
            out.push_back(stations()[i]);
    return out;
}

} // namespace radiolist
