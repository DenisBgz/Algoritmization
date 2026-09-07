#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>
#include <xmllite.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

using Coord = std::pair<double, double>;

struct Node { std::string id; double x=0, y=0; };
struct Edge { int from, to; double length; std::string key, name; std::vector<Coord> geometry; };
struct Graph {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> adj;
    std::unordered_map<std::string,int> index;
    int add_node(const std::string& id, double x=0, double y=0) {
        if(index.count(id)) throw std::runtime_error("Duplicate node: "+id);
        int i=static_cast<int>(nodes.size());
        nodes.push_back({id,x,y}); adj.emplace_back(); index[id]=i; return i;
    }
    void add_edge(int u,int v,double w,std::string key="0",std::string name="") {
        if(u<0 || v<0 || u>=static_cast<int>(nodes.size()) || v>=static_cast<int>(nodes.size()))
            throw std::runtime_error("Invalid edge endpoint");
        if(!std::isfinite(w) || w<0) throw std::runtime_error("Dijkstra requires finite nonnegative weights");
        adj[u].push_back(static_cast<int>(edges.size()));
        edges.push_back({u,v,w,std::move(key),std::move(name), {}});
    }
};
struct Result {
    double distance=std::numeric_limits<double>::infinity();
    std::vector<int> path, edge_path;
    std::vector<std::string> street_names;
    size_t settled=0;
};

Result dijkstra(const Graph& g,int start,int end) {
    if(start<0 || end<0 || start>=static_cast<int>(g.nodes.size()) || end>=static_cast<int>(g.nodes.size()))
        throw std::runtime_error("Invalid start/end");
    const double inf=std::numeric_limits<double>::infinity();
    std::vector<double> dist(g.nodes.size(),inf);
    std::vector<int> previous_edge(g.nodes.size(),-1);
    using Item=std::pair<double,int>;
    std::priority_queue<Item,std::vector<Item>,std::greater<Item>> q;
    dist[start]=0; q.push({0,start});
    Result result;
    while(!q.empty()) {
        auto [du,u]=q.top(); q.pop();
        if(du!=dist[u]) continue; // Устаревшая запись в очереди.
        ++result.settled;
        if(u==end) break; // Кратчайшая метка цели уже окончательная.
        for(int ei:g.adj[u]) {
            const Edge& e=g.edges[ei];
            const double candidate=du+e.length;
            if(candidate<dist[e.to]) {
                dist[e.to]=candidate;
                previous_edge[e.to]=ei; // Сохраняем конкретное параллельное ребро.
                q.push({candidate,e.to});
            }
        }
    }
    result.distance=dist[end];
    if(!std::isfinite(dist[end])) return result;
    for(int v=end;v!=start;) {
        int ei=previous_edge[v];
        if(ei<0) throw std::runtime_error("Broken predecessor chain");
        result.path.push_back(v); result.edge_path.push_back(ei);
        v=g.edges[ei].from;
    }
    result.path.push_back(start);
    std::reverse(result.path.begin(),result.path.end());
    std::reverse(result.edge_path.begin(),result.edge_path.end());
    for(int ei:result.edge_path) {
        const auto& name=g.edges[ei].name;
        if(!name.empty() && (result.street_names.empty() || result.street_names.back()!=name))
            result.street_names.push_back(name);
    }
    return result;
}

// Чтение GraphML средствами Windows. Отдельная библиотека TinyXML-2 не нужна.
using Microsoft::WRL::ComPtr;

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8 text");
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string utf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

void check_hr(HRESULT hr, const char* operation) {
    if (FAILED(hr)) throw std::runtime_error(std::string(operation) + " failed, HRESULT=" + std::to_string(hr));
}

std::string xml_name(IXmlReader* reader) {
    const WCHAR* value = nullptr;
    check_hr(reader->GetLocalName(&value, nullptr), "GetLocalName");
    return utf8(value ? value : L"");
}

std::string xml_value(IXmlReader* reader) {
    const WCHAR* value = nullptr;
    UINT length = 0;
    check_hr(reader->GetValue(&value, &length), "GetValue");
    return value ? utf8(std::wstring(value, length)) : "";
}

std::string xml_attr(IXmlReader* reader, const wchar_t* name, const std::string& fallback = "") {
    HRESULT hr = reader->MoveToAttributeByName(name, nullptr);
    if (hr == S_FALSE) return fallback;
    check_hr(hr, "MoveToAttributeByName");
    std::string value = xml_value(reader);
    check_hr(reader->MoveToElement(), "MoveToElement");
    return value;
}

double number(const std::string& s) {
    size_t used = 0;
    double value = std::stod(s, &used);
    while (used < s.size() && std::isspace(static_cast<unsigned char>(s[used]))) ++used;
    if (used != s.size() || !std::isfinite(value)) throw std::runtime_error("Invalid numeric value: " + s);
    return value;
}

std::vector<Coord> read_geometry(std::string text) {
    if (text.empty()) return {};
    auto first = text.find('('), last = text.rfind(')');
    if (text.substr(0, 10) != "LINESTRING" || first == std::string::npos || last <= first)
        throw std::runtime_error("Expected LINESTRING geometry in OSMnx GraphML");
    text = text.substr(first + 1, last - first - 1);
    std::replace(text.begin(), text.end(), ',', ' ');
    std::istringstream input(text);
    std::vector<Coord> points;
    double x, y;
    while (input >> x) {
        if (!(input >> y) || !std::isfinite(x) || !std::isfinite(y)) throw std::runtime_error("Invalid road geometry");
        points.push_back({x, y});
    }
    return points;
}

Graph read_graphml(const std::filesystem::path& file) {
    ComPtr<IStream> input;
    check_hr(SHCreateStreamOnFileEx(file.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
                                  FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, input.GetAddressOf()), "Open GraphML");
    ComPtr<IXmlReader> reader;
    check_hr(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.GetAddressOf()), nullptr), "CreateXmlReader");
    check_hr(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit), "Disable DTD");
    check_hr(reader->SetInput(input.Get()), "SetInput");

    struct Key { std::string scope, name; };
    struct PendingEdge {
        std::string from, to, key, name, geometry;
        double length;
        bool directed;
    };
    std::unordered_map<std::string, Key> keys;
    std::unordered_map<std::string, std::string> data;
    std::vector<PendingEdge> pending;
    Graph graph;
    std::string current, id, source, target, edge_key, data_key, data_text;
    bool default_directed = true, directed = true, in_data = false, has_root = false, has_graph = false;

    auto finish_item = [&]() {
        if (current == "node") {
            if (id.empty() || !data.count("x") || !data.count("y")) throw std::runtime_error("Node requires id, x and y");
            graph.add_node(id, number(data.at("x")), number(data.at("y")));
        } else if (current == "edge") {
            if (!data.count("length")) throw std::runtime_error("Edge requires length in metres");
            pending.push_back({source, target, edge_key, data["name"], data["geometry"], number(data.at("length")), directed});
        }
        current.clear();
        data.clear();
    };

    XmlNodeType type;
    HRESULT hr;
    while ((hr = reader->Read(&type)) == S_OK) {
        if (type == XmlNodeType_Element) {
            std::string tag = xml_name(reader.Get());
            if (tag == "graphml") has_root = true;
            else if (tag == "key") {
                keys[xml_attr(reader.Get(), L"id")] = {xml_attr(reader.Get(), L"for", "all"), xml_attr(reader.Get(), L"attr.name")};
            } else if (tag == "graph") {
                if (has_graph) throw std::runtime_error("Only one graph is supported");
                has_graph = true;
                std::string mode = xml_attr(reader.Get(), L"edgedefault");
                if (mode != "directed" && mode != "undirected") throw std::runtime_error("Invalid edgedefault");
                default_directed = mode == "directed";
            } else if (tag == "node" || tag == "edge") {
                if (!current.empty()) throw std::runtime_error("Nested graph elements are unsupported");
                current = tag;
                data.clear();
                id = xml_attr(reader.Get(), L"id");
                source = xml_attr(reader.Get(), L"source");
                target = xml_attr(reader.Get(), L"target");
                edge_key = xml_attr(reader.Get(), L"id", "0");
                std::string mode = xml_attr(reader.Get(), L"directed", default_directed ? "true" : "false");
                if (mode != "true" && mode != "false" && mode != "0" && mode != "1") throw std::runtime_error("Invalid directed attribute");
                directed = mode == "true" || mode == "1";
                if (reader->IsEmptyElement()) finish_item();
            } else if (tag == "data" && !current.empty()) {
                data_key = xml_attr(reader.Get(), L"key");
                data_text.clear();
                in_data = !reader->IsEmptyElement();
            }
        } else if (type == XmlNodeType_Text || type == XmlNodeType_CDATA || type == XmlNodeType_Whitespace) {
            if (in_data) data_text += xml_value(reader.Get());
        } else if (type == XmlNodeType_EndElement) {
            std::string tag = xml_name(reader.Get());
            if (tag == "data" && in_data) {
                auto k = keys.find(data_key);
                if (k != keys.end() && (k->second.scope == current || k->second.scope == "all"))
                    data[k->second.name] = data_text;
                in_data = false;
            } else if (tag == current) finish_item();
        }
    }
    check_hr(hr, "Read XML");
    if (!has_root || !has_graph || graph.nodes.empty()) throw std::runtime_error("Empty or invalid GraphML graph");
    for (const auto& e : pending) {
        auto u = graph.index.find(e.from), v = graph.index.find(e.to);
        if (u == graph.index.end() || v == graph.index.end()) throw std::runtime_error("Unknown edge endpoint");
        auto geometry = read_geometry(e.geometry);
        graph.add_edge(u->second, v->second, e.length, e.key, e.name);
        graph.edges.back().geometry = geometry;
        if (!e.directed && u->second != v->second) {
            graph.add_edge(v->second, u->second, e.length, e.key, e.name);
            std::reverse(geometry.begin(), geometry.end());
            graph.edges.back().geometry = std::move(geometry);
        }
    }
    return graph;
}

std::string json_string(const std::string& value) {
    std::ostringstream s;s<<'"';
    for(unsigned char c:value) {
        if(c=='"'||c=='\\')s<<'\\'<<c;
        else if(c<32)s<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;
        else s<<c;
    }
    s<<'"';return s.str();
}
void self_test() {
    auto check=[](bool ok){if(!ok)throw std::runtime_error("Self-test failed");};
    Graph g;for(auto id:{"A","B","C","D","X"})g.add_node(id);
    for(auto [u,v,w]:std::vector<std::tuple<int,int,double>>{{0,1,3},{0,2,5},{0,3,2},{1,2,4},{1,3,6},{2,3,1}}) {
        g.add_edge(u,v,w);g.add_edge(v,u,w);
    }
    auto r=dijkstra(g,0,2);check(r.distance==3 && r.path==std::vector<int>({0,3,2}));
    check(dijkstra(g,0,4).path.empty());check(dijkstra(g,0,0).path==std::vector<int>({0}));
    Graph h;h.add_node("A");h.add_node("B");h.add_node("C");
    h.add_edge(0,1,5,"0","long");h.add_edge(0,1,0,"1","zero");h.add_edge(1,2,2,"0","finish");
    auto p=dijkstra(h,0,2);check(p.distance==2 && h.edges[p.edge_path[0]].key=="1");
    check(p.street_names==std::vector<std::string>({"zero","finish"}));
    check(dijkstra(h,2,0).path.empty());
    bool rejected=false;try{h.add_edge(0,2,-1);}catch(const std::exception&){rejected=true;}check(rejected);
    std::cout<<"SELF-TEST OK: variant, identity, unreachable, directed, parallel, zero weight, street names, negative rejection\n";
    std::cout<<"Вариант 1: A -> D -> C; длина = 3 условные единицы\n";
}
int run_query(int argc,char** argv, double* median_out = nullptr) {
    try {
        if(argc==2 && std::string(argv[1])=="--self-test"){self_test();return 0;}
        if(argc<5 || argc>6) {
            std::cerr<<"Usage: lab5 graph.graphml start_id end_id output_prefix [repeats=101]\n       lab5 --self-test\n";return 2;
        }
        int repeats=argc==6?std::stoi(argv[5]):101;
        if(repeats<1 || repeats>100000)throw std::runtime_error("Invalid repeat count");
        auto begin=std::chrono::steady_clock::now();Graph g=read_graphml(std::filesystem::u8path(argv[1]));
        double load_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        int start=g.index.at(argv[2]),end=g.index.at(argv[3]);
        Result result;
        for(int i=0;i<10;++i)result=dijkstra(g,start,end); // Разогрев без включения в выборку.
        std::vector<double> samples;samples.reserve(repeats);
        for(int i=0;i<repeats;++i) {
            auto t0=std::chrono::steady_clock::now();
            Result current=dijkstra(g,start,end);
            auto t1=std::chrono::steady_clock::now();
            samples.push_back(std::chrono::duration<double,std::milli>(t1-t0).count());
            result=std::move(current); // Присваивание и вывод не входят в замер.
        }
        std::string prefix=argv[4];std::ofstream csv(std::filesystem::u8path(prefix+"_timings.csv"));
        if(!csv)throw std::runtime_error("Cannot create timings file");
        csv<<"run,algorithm_ms\n"<<std::setprecision(12);
        for(int i=0;i<repeats;++i)csv<<i+1<<','<<samples[i]<<'\n';
        auto sorted=samples;std::sort(sorted.begin(),sorted.end());
        double median=sorted[repeats/2];if(repeats%2==0)median=(sorted[repeats/2-1]+median)/2;
        if (median_out) *median_out = median;
        std::ofstream out(std::filesystem::u8path(prefix+".json"));if(!out)throw std::runtime_error("Cannot create JSON");
        out<<std::setprecision(15)<<"{\n\"nodes\":"<<g.nodes.size()<<",\"edges\":"<<g.edges.size()
           <<",\"load_ms\":"<<load_ms<<",\"repeats\":"<<repeats<<",\"warmups\":10,\"median_ms\":"<<median
           <<",\"min_ms\":"<<sorted.front()<<",\"max_ms\":"<<sorted.back()<<",\"settled\":"<<result.settled
           <<",\"distance_m\":";
        if(std::isfinite(result.distance))out<<result.distance;else out<<"null";
        out<<",\n\"path\":[";
        for(size_t i=0;i<result.path.size();++i){if(i)out<<',';out<<json_string(g.nodes[result.path[i]].id);}
        out<<"],\n\"street_names\":[";
        for(size_t i=0;i<result.street_names.size();++i){if(i)out<<',';out<<json_string(result.street_names[i]);}
        out<<"],\n\"route_edges\":[";
        for(size_t i=0;i<result.edge_path.size();++i) {
            const auto& e=g.edges[result.edge_path[i]];if(i)out<<',';
            out<<"{\"u\":"<<json_string(g.nodes[e.from].id)<<",\"v\":"<<json_string(g.nodes[e.to].id)
               <<",\"key\":"<<json_string(e.key)<<",\"length_m\":"<<e.length<<",\"name\":"<<json_string(e.name)<<'}';
        }
        out<<"]\n}\n";
        std::ofstream edge_csv(std::filesystem::u8path(prefix + "_edges.csv"));
        if (!edge_csv) throw std::runtime_error("Cannot create edge CSV");
        edge_csv << "step,source,target,edge_key,street,length_m,cumulative_m\n" << std::setprecision(15);
        auto csv_field = [](const std::string& value) {
            std::string out = "\"";
            for (char c : value) { if (c == '"') out += '"'; out += c; }
            return out + "\"";
        };
        double accumulated = 0;
        for (size_t i = 0; i < result.edge_path.size(); ++i) {
            const auto& e = g.edges[result.edge_path[i]];
            accumulated += e.length;
            edge_csv << i + 1 << ',' << csv_field(g.nodes[e.from].id) << ',' << csv_field(g.nodes[e.to].id)
                     << ',' << csv_field(e.key) << ',' << csv_field(e.name) << ',' << e.length << ',' << accumulated << '\n';
        }
        std::cout<<"N="<<g.nodes.size()<<" M="<<g.edges.size()<<" distance_m="<<std::setprecision(12)<<result.distance
                 <<" median_ms="<<median<<" settled="<<result.settled<<"\n";
        return result.path.empty()?3:0;
    }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<'\n';return 1;}
}

// Графическое окно и PNG средствами GDI+, входящими в Windows.
// Результат рисуется тем же кодом и при сохранении, и при показе в окне.
void draw_text(Gdiplus::Graphics& painter, const std::wstring& text,
               float x, float y, float size = 22, bool bold = false) {
    Gdiplus::Font font(L"Arial", size, bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 25, 35, 45));
    painter.DrawString(text.c_str(), -1, &font, Gdiplus::PointF(x, y), &brush);
}

CLSID png_encoder() {
    UINT count = 0, bytes = 0;
    Gdiplus::GetImageEncodersSize(&count, &bytes);
    if (!bytes) throw std::runtime_error("No image encoders");
    std::vector<unsigned char> memory(bytes);
    auto info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(memory.data());
    if (Gdiplus::GetImageEncoders(count, bytes, info) != Gdiplus::Ok) throw std::runtime_error("Cannot read PNG encoder");
    for (UINT i = 0; i < count; ++i) if (std::wstring(info[i].MimeType) == L"image/png") return info[i].Clsid;
    throw std::runtime_error("PNG encoder unavailable");
}

std::string fixed(double value, int decimals = 3) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << value;
    return out.str();
}

void save_visualization(const Graph& graph, const Result* route,
                        const std::filesystem::path& filename,
                        const std::string& title,
                        const std::vector<std::pair<int, std::string>>& marks) {
    const int width = route ? 1500 : 1100;
    const int height = route ? 1000 : 1450;
    Gdiplus::Bitmap canvas(width, height, PixelFormat32bppARGB);
    if (canvas.GetLastStatus() != Gdiplus::Ok) throw std::runtime_error("Cannot create image");
    Gdiplus::Graphics painter(&canvas);
    painter.Clear(Gdiplus::Color(255, 255, 255, 255));
    painter.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    painter.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    draw_text(painter, wide(title), 40, 22, 29, true);
    std::string subtitle = route ? "Длина: " + fixed(route->distance / 1000) + " км" :
        "Вершины: " + std::to_string(graph.nodes.size()) + "    Рёбра: " + std::to_string(graph.edges.size());
    draw_text(painter, wide(subtitle), 40, 65, 23);

    // Эквидистантное локальное отображение: масштаб долгот умножается на cos(phi).
    // Эта формула влияет только на рисунок, длины берутся из GraphML length.
    const double longitude_scale = std::cos(55.75 * 3.14159265358979323846 / 180.0);
    double min_x = 1e100, min_y = 1e100, max_x = -1e100, max_y = -1e100;
    auto include = [&](double x, double y) {
        x *= longitude_scale;
        min_x = std::min(min_x, x); max_x = std::max(max_x, x);
        min_y = std::min(min_y, y); max_y = std::max(max_y, y);
    };
    if (route) {
        for (int i : route->path) include(graph.nodes[i].x, graph.nodes[i].y);
        for (int i : route->edge_path) for (const auto& xy : graph.edges[i].geometry) include(xy.first, xy.second);
    } else {
        for (const auto& n : graph.nodes) include(n.x, n.y);
    }
    double dx = std::max(max_x - min_x, 0.002), dy = std::max(max_y - min_y, 0.002);
    min_x -= dx * 0.10; max_x += dx * 0.10;
    min_y -= dy * 0.10; max_y += dy * 0.10;
    const float left = 45, top = 120, box_w = static_cast<float>(width) - 90, box_h = static_cast<float>(height) - 205;
    double scale = std::min(box_w / (max_x - min_x), box_h / (max_y - min_y));
    double mid_x = (min_x + max_x) / 2, mid_y = (min_y + max_y) / 2;
    auto project = [&](double x, double y) {
        return Gdiplus::PointF(static_cast<float>(left + box_w / 2 + (x * longitude_scale - mid_x) * scale),
                              static_cast<float>(top + box_h / 2 - (y - mid_y) * scale));
    };
    painter.SetClip(Gdiplus::RectF(left, top, box_w, box_h));
    Gdiplus::Pen road_pen(route ? Gdiplus::Color(255, 199, 205, 210) : Gdiplus::Color(255, 98, 114, 125), route ? 1.15f : 0.85f);
    Gdiplus::Pen path_pen(Gdiplus::Color(255, 181, 34, 54), 4.0f);
    auto draw_edge = [&](const Edge& edge, Gdiplus::Pen& pen) {
        std::vector<Gdiplus::PointF> line;
        if (edge.geometry.size() >= 2) {
            line.reserve(edge.geometry.size());
            for (const auto& xy : edge.geometry) line.push_back(project(xy.first, xy.second));
        } else {
            const Node& a = graph.nodes[edge.from]; const Node& b = graph.nodes[edge.to];
            line = {project(a.x, a.y), project(b.x, b.y)};
        }
        painter.DrawLines(&pen, line.data(), static_cast<int>(line.size()));
    };
    for (const auto& edge : graph.edges) draw_edge(edge, road_pen);
    if (route) for (int i : route->edge_path) draw_edge(graph.edges[i], path_pen);
    painter.ResetClip();
    for (const auto& [i, label] : marks) {
        auto point = project(graph.nodes[i].x, graph.nodes[i].y);
        Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::Pen outline(Gdiplus::Color(255, 25, 51, 68), 2);
        float radius = route ? 18.0f : 13.0f;
        painter.FillEllipse(&white, point.X - radius, point.Y - radius, radius * 2, radius * 2);
        painter.DrawEllipse(&outline, point.X - radius, point.Y - radius, radius * 2, radius * 2);
        draw_text(painter, wide(label), point.X - radius * .50f, point.Y - radius * .75f, radius * 1.35f, true);
    }
    draw_text(painter, L"Серый — дорожная сеть    Красный — кратчайший путь", 40, static_cast<float>(height - 65), 19);
    draw_text(painter, L"Данные © OpenStreetMap contributors · ODbL 1.0", 40, static_cast<float>(height - 36), 18);
    CLSID encoder = png_encoder();
    if (canvas.Save(filename.c_str(), &encoder, nullptr) != Gdiplus::Ok) throw std::runtime_error("Cannot save PNG");
}

struct Gallery {
    std::vector<std::filesystem::path> files;
    size_t current = 0;
    std::unique_ptr<Gdiplus::Bitmap> picture;
    void load() {
        picture = std::make_unique<Gdiplus::Bitmap>(files[current].c_str());
        if (picture->GetLastStatus() != Gdiplus::Ok) throw std::runtime_error("Cannot open visualization");
    }
};

LRESULT CALLBACK gallery_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto gallery = reinterpret_cast<Gallery*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
    } else if (message == WM_KEYDOWN && gallery) {
        if (wparam == VK_ESCAPE) DestroyWindow(window);
        else {
            if (wparam == VK_RIGHT || wparam == VK_SPACE) gallery->current = (gallery->current + 1) % gallery->files.size();
            else if (wparam == VK_LEFT) gallery->current = (gallery->current + gallery->files.size() - 1) % gallery->files.size();
            else if (wparam >= '1' && wparam <= '4') gallery->current = std::min(static_cast<size_t>(wparam - '1'), gallery->files.size() - 1);
            try { gallery->load(); } catch (...) { DestroyWindow(window); return 0; }
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    } else if (message == WM_SIZE) {
        InvalidateRect(window, nullptr, FALSE); return 0;
    } else if (message == WM_PAINT && gallery && gallery->picture) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT area; GetClientRect(window, &area);
        Gdiplus::Bitmap buffer(area.right > 0 ? area.right : 1, area.bottom > 0 ? area.bottom : 1);
        Gdiplus::Graphics g(&buffer); g.Clear(Gdiplus::Color(255, 255, 255, 255));
        double scale = std::min(static_cast<double>(area.right) / gallery->picture->GetWidth(),
                                static_cast<double>(std::max(area.bottom - 38L, 1L)) / gallery->picture->GetHeight());
        float w = static_cast<float>(gallery->picture->GetWidth() * scale), h = static_cast<float>(gallery->picture->GetHeight() * scale);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(gallery->picture.get(), (area.right - w) / 2, 0.0f, w, h);
        draw_text(g, L"1–4 или стрелки — смена рисунка     Esc — закрыть", 15, static_cast<float>(area.bottom - 30), 18);
        Gdiplus::Graphics screen(dc); screen.DrawImage(&buffer, 0, 0);
        EndPaint(window, &paint); return 0;
    } else if (message == WM_ERASEBKGND) return 1;
    else if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}

void visualize_path_with_network(const std::vector<std::filesystem::path>& figures) {
    Gallery gallery{figures}; gallery.load();
    WNDCLASSW wc{}; wc.lpfnWndProc = gallery_proc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Lab5MoscowViewer"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register window");
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"Лабораторная работа 5 — Москва — вариант 1",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1150, 950,
                                  nullptr, nullptr, wc.hInstance, &gallery);
    if (!window) throw std::runtime_error("Cannot create visualization window");
    ShowWindow(window, SW_SHOW); UpdateWindow(window);
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
}

// Автоматический запуск трёх маршрутов, совпадающих с контрольными точками отчёта.
// Можно изменить OSM ID ниже или вызвать программу с собственными start_id/end_id.
int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct ComGuard { bool active; ~ComGuard() { if (active) CoUninitialize(); } } com_guard{SUCCEEDED(com)};
    try {
        if (FAILED(com) && com != RPC_E_CHANGED_MODE) check_hr(com, "CoInitializeEx");
        if (argc == 2 && std::wstring(argv[1]) == L"--self-test") { self_test(); return 0; }
        if (argc >= 5 && argv[1][0] != L'-') {
            std::vector<std::string> args;
            for (int i = 0; i < argc; ++i) args.push_back(utf8(argv[i]));
            std::vector<char*> pointers;
            for (auto& s : args) pointers.push_back(s.data());
            return run_query(argc, pointers.data());
        }
        bool show_window = true;
        std::filesystem::path graph_file = L"moscow_road_network.graphml";
        std::filesystem::path output = L"results";
        if (!std::filesystem::exists(graph_file) && std::filesystem::exists(L"data/moscow_road_network.graphml"))
            graph_file = L"data/moscow_road_network.graphml";
        if (!std::filesystem::exists(graph_file)) {
            wchar_t executable[32768]{};
            DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
            if (length > 0 && length < 32768) {
                auto nearby = std::filesystem::path(executable).parent_path() / L"moscow_road_network.graphml";
                if (std::filesystem::exists(nearby)) graph_file = nearby;
            }
        }
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--no-window") show_window = false;
            else if (arg == L"--graph" && i + 1 < argc) graph_file = argv[++i];
            else if (arg == L"--out" && i + 1 < argc) output = argv[++i];
            else throw std::runtime_error("Usage: lab5 [--graph file.graphml] [--out folder] [--no-window]");
        }
        std::cout << "Лабораторная работа 5. Вариант 1. Москва.\n";
        std::cout << "Учебный граф: A-B=3, A-C=5, A-D=2, B-C=4, B-D=6, C-D=1.\n";
        self_test();
        if (!std::filesystem::exists(graph_file)) {
            std::cerr << "\nДля расчёта Москвы положите moscow_road_network.graphml рядом с программой\n"
                         "или укажите путь: lab5.exe --graph \"полный путь к файлу.graphml\"\n";
            return 1;
        }
        Graph graph = read_graphml(graph_file);
        std::cout << "Вершин: " << graph.nodes.size() << ", ориентированных рёбер: " << graph.edges.size() << "\n";
        struct Point { std::string label, street, id; };
        const std::vector<Point> points = {
            {"A", "Тверская улица", "253122999"},
            {"B", "улица Новый Арбат", "253044204"},
            {"C", "Таганская улица", "60775948"}
        };
        std::vector<std::pair<int, std::string>> marks;
        for (const auto& p : points) {
            auto n = graph.index.find(p.id);
            if (n == graph.index.end()) throw std::runtime_error("Missing control point " + p.id + ". Use the saved Moscow GraphML snapshot.");
            marks.push_back({n->second, p.label});
            const Node& node = graph.nodes[n->second];
            std::cout << p.label << " = " << p.street << ", OSM ID " << p.id
                      << ", широта " << std::setprecision(9) << node.y << ", долгота " << node.x << "\n";
        }
        std::filesystem::create_directories(output / "figures");
        Gdiplus::GdiplusStartupInput startup;
        ULONG_PTR token = 0;
        if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) throw std::runtime_error("GDI+ startup failed");
        struct DrawingGuard { ULONG_PTR token; ~DrawingGuard() { Gdiplus::GdiplusShutdown(token); } } drawing_guard{token};
        std::vector<std::filesystem::path> figures;
        auto network_png = output / "figures/moscow_network.png";
        save_visualization(graph, nullptr, network_png, "Дорожная сеть Москвы", marks);
        figures.push_back(network_png);
        std::ofstream table(output / "table1.csv", std::ios::binary);
        if (!table) throw std::runtime_error("Cannot create table1.csv");
        table << "\xEF\xBB\xBFstart;destination;distance_km;figure;median_seconds;median_ms\n";
        for (auto [a, b] : std::vector<std::pair<int, int>>{{0, 1}, {1, 2}, {0, 2}}) {
            const Point& start = points[a]; const Point& end = points[b];
            std::string code = start.label + end.label;
            std::vector<std::string> args = {"lab5", graph_file.u8string(), start.id, end.id, (output / code).u8string(), "101"};
            std::vector<char*> pointers;
            for (auto& arg : args) pointers.push_back(arg.data());
            double median = 0;
            std::cout << "\nМаршрут " << start.label << " -> " << end.label << ":\n";
            int status = run_query(static_cast<int>(pointers.size()), pointers.data(), &median);
            if (status) throw std::runtime_error("Route calculation failed: " + code);
            Result route = dijkstra(graph, marks[a].first, marks[b].first);
            std::cout << "Расстояние: " << fixed(route.distance / 1000, 6) << " км\n"
                      << "Медиана времени: " << fixed(median, 4) << " мс, 101 запуск после 10 прогревов.\nУлицы:\n";
            for (const auto& name : route.street_names) std::cout << "  " << name << "\n";
            auto png = output / ("figures/route_" + code + ".png");
            save_visualization(graph, &route, png, "Маршрут " + start.label + " -> " + end.label,
                               {marks[a], marks[b]});
            figures.push_back(png);
            table << start.street << ';' << end.street << ';' << fixed(route.distance / 1000, 6) << ';'
                  << png.filename().u8string() << ';' << fixed(median / 1000, 9) << ';' << fixed(median, 6) << '\n';
        }
        std::cout << "\nГотово. Карты PNG, маршруты JSON и реальные замеры CSV сохранены в " << output.u8string()
                  << ".\nВ окне: 1–4 или стрелки — выбор карты, Esc — выход.\n";
        if (show_window) visualize_path_with_network(figures);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}
