/*

 Copyright (C) 2009 Anton Burdinuk

 clark15b@gmail.com

*/



#include "../demux/demux.h"
#include "ts.h"
#include "mpls.h"

std::vector<TSDemux::STREAM_PKT*> get_demux_packets(std::string buffer) {
    ts::demuxer demuxer;

    demuxer.av_only=true;
    demuxer.channel=0;
    demuxer.pes_output=false;
    demuxer.dst="";
    demuxer.verb=false;
    demuxer.es_parse=true;

    demuxer.demux_buffer(buffer);

    demuxer.reset();

    demuxer.show();

}


namespace ts
{
    class ts_file_info
    {
    public:
        std::string filename;
        u_int64_t first_dts;
        u_int64_t first_pts;
        u_int64_t last_pts;

        ts_file_info(void):first_dts(0),first_pts(0),last_pts(0) {}
    };

#ifdef _WIN32
    inline int strcasecmp(const char* s1,const char* s2) { return lstrcmpi(s1,s2); }
#endif

    int scan_dir(const char* path,std::list<std::string>& l);
    void load_playlist(const char* path,std::list<std::string>& l,std::map<int,std::string>& d);
    int get_clip_number_by_filename(const std::string& s);

    bool is_ts_filename(const std::string& s)
    {
        if(!s.length())
            return false;

        if(s[s.length()-1]=='/' || s[s.length()-1]=='\\')
            return false;

        std::string::size_type n=s.find_last_of('.');

        if(n==std::string::npos)
            return false;

        std::string ext=s.substr(n+1);

        if(!strcasecmp(ext.c_str(),"ts") || !strcasecmp(ext.c_str(),"m2ts"))
            return true;

        return false;
    }

    std::string trim_slash(const std::string& s)
    {
        const char* p=s.c_str()+s.length();

        while(p>s.c_str() && (p[-1]=='/' || p[-1]=='\\'))
            p--;

        return s.substr(0,p-s.c_str());
    }

    const char* timecode_to_time(u_int32_t timecode)
    {
        static char buf[128];

        int msec=timecode%1000;
        timecode/=1000;

        int sec=timecode%60;
        timecode/=60;

        int min=timecode%60;
        timecode/=60;

        sprintf(buf,"%.2i:%.2i:%.2i.%.3i",(int)timecode,min,sec,msec);

        return buf;
    }

}

#ifdef _WIN32
int ts::scan_dir(const char* path,std::list<std::string>& l)
{
    _finddata_t fileinfo;

    intptr_t dir=_findfirst((std::string(path)+"\\*.*").c_str(),&fileinfo);

    if(dir==-1)
        perror(path);
    else
    {
        while(!_findnext(dir,&fileinfo))
            if(!(fileinfo.attrib&_A_SUBDIR) && *fileinfo.name!='.')
            {
                char p[512];

                int n=sprintf(p,"%s\\%s",path,fileinfo.name);

                l.push_back(std::string(p,n));
            }
    }

    _findclose(dir);

    return l.size();
}
#else
int ts::scan_dir(const char* path,std::list<std::string>& l)
{
    DIR* dir=opendir(path);

    if(!dir)
        perror(path);
    else
    {
        dirent* d;

        while((d=readdir(dir)))
        {
            if (d->d_type == DT_UNKNOWN) {
                char p[512];

                if (snprintf(p, sizeof(p), "%s/%s", path, d->d_name) > 0) {
                    struct stat st;

                    if (stat(p, &st) != -1) {
                        if (S_ISREG(st.st_mode))
                            d->d_type = DT_REG;
                        else if (S_ISLNK(st.st_mode))
                            d->d_type = DT_LNK;
                    }
                }
            }
            if(*d->d_name!='.' && (d->d_type==DT_REG || d->d_type==DT_LNK))
            {
                char p[512];

                int n=sprintf(p,"%s/%s",path,d->d_name);

                l.push_back(std::string(p,n));
            }
        }

        closedir(dir);
    }

    return l.size();
}
#endif

