#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <opencv2/geometry.hpp>
#include <filesystem>  
using namespace std;

const string DEFAULT_IMAGE = "images/outpost_frame.png";
const int   S_MIN = 100;                 // 饱和度下限
const int   V_MIN = 120;                 // 亮度下限
const int   DIFF_THRESH = 60;            // 红蓝差阈值
const cv::Size KERNEL_SIZE(20, 60);      // 闭运算的核
const double MIN_AREA = 20;              // 灯条最小面积
const float MIN_ASPECT = 3.0f;           // 灯条最小长宽比
const float MIN_LEN_RATIO = 0.7f;        // 两灯条长度比下限
const float MAX_DY_RATIO = 0.5f;         // 两灯条竖直错开 / 平均长度 的上限
const float MIN_DIST_RATIO = 1.5f;       // 间距 / 平均长度 的下限
const float MAX_DIST_RATIO = 4.0f;       // 间距 / 平均长度 的上限
const float MAX_ANGLE_DIFF = 15.0f;      // 两灯条倾角差上限（度）
const float MIN_ARMOR_ASPECT = 1.2f;     // 装甲板外接框 宽/高 下限
const float MAX_ARMOR_ASPECT = 5.0f;     // 装甲板外接框 宽/高 上限
const float MAX_OVERLAP = 0.5f;          // 两个装甲板外接框重叠比例上限

// 灯条
struct LightBar {
    cv::RotatedRect rect;   
    float length;           
    float width;            
    string color;
    float angle;
};

// 装甲板
struct Armor {
    string color;
    cv::Rect box;
    cv::Point2f center;
    cv::Point2f corners[4];
    int leftBar;            
    int rightBar;           
    bool valid;
};

// 中间图
struct DebugInfo {
    cv::Mat mask;
    cv::Mat cleanMask;
    cv::Mat candidates;
};

// ================= 函数 =================


void BarEnds(const cv::RotatedRect& r, cv::Point2f& top, cv::Point2f& bottom) {
    cv::Point2f p[4];
    r.points(p);
    sort(p, p + 4, [](const cv::Point2f& a, const cv::Point2f& b) { return a.y < b.y; });
    top = (p[0] + p[1]) * 0.5f;
    bottom = (p[2] + p[3]) * 0.5f;
}


bool saveImage(const string& path, const cv::Mat& img) {
    if (!cv::imwrite(path, img)) {
        cerr << "图片保存失败: " << path << endl;
        return false;
    }
    return true;
}

// 颜色提取
cv::Mat makeMask(const cv::Mat& image) {
    cv::Mat hsv, m1, m2, maskHSV, maskDiff;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);                                   
    cv::inRange(hsv, cv::Scalar(0, S_MIN, V_MIN), cv::Scalar(12, 255, 255), m1);   
    cv::inRange(hsv, cv::Scalar(165, S_MIN, V_MIN), cv::Scalar(180, 255, 255), m2);
    maskHSV = m1 | m2;                                                            

    vector<cv::Mat> ch;
    cv::split(image, ch);                                                          
    cv::Mat diff = ch[2] - ch[0];                                                 
    cv::threshold(diff, maskDiff, DIFF_THRESH, 255, cv::THRESH_BINARY);            

    cv::Mat mask = maskHSV & maskDiff;                                             
    return mask;
}

// 闭运算
cv::Mat closeMask(const cv::Mat& mask) {
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, KERNEL_SIZE);       
    cv::Mat result;
    cv::morphologyEx(mask, result, cv::MORPH_CLOSE, kernel);                       
    return result;
}

// 找灯条

vector<LightBar> findLightBars(const cv::Mat& cleanMask, const string& color) {
    vector<vector<cv::Point>> contours;
    cv::findContours(cleanMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    vector<LightBar> bars;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area < MIN_AREA) continue;                        

        cv::RotatedRect rect = cv::minAreaRect(contours[i]);
        float len = max(rect.size.width, rect.size.height);    
        float wid = min(rect.size.width, rect.size.height);    
        if (wid < 1e-3f) continue;                            
        if (len / wid < MIN_ASPECT) continue;                  

        LightBar bar;                                          
        bar.color = color;
        bar.rect = rect;
        bar.length = len;
        bar.width = wid;
        cv::Point2f top, bottom;
        BarEnds(rect, top, bottom);
        bar.angle = atan2(bottom.x - top.x, bottom.y - top.y) * 180.0f / (float)CV_PI;
        bars.push_back(bar);

        cout << "灯条" << bars.size() - 1 << " 面积=" << area
            << " 长=" << len << " 宽=" << wid
            << " 中心=(" << rect.center.x << "," << rect.center.y << ")" << endl;
    }
    return bars;
}

// 画灯条候选图
void drawBars(cv::Mat& img, const vector<LightBar>& bars) {
    for (size_t id = 0; id < bars.size(); id++) {
        cv::Point2f pts[4];
        bars[id].rect.points(pts);
        for (int k = 0; k < 4; k++) {
            cv::line(img, pts[k], pts[(k + 1) % 4], cv::Scalar(0, 255, 0), 1);
        }
        cv::circle(img, bars[id].rect.center, 2, cv::Scalar(0, 255, 255), -1);
        cv::putText(img, to_string(id), bars[id].rect.center + cv::Point2f(5, -5),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    }
}

// 灯条配对

vector<pair<int, int>> matchBars(const vector<LightBar>& bars) {
    vector<pair<int, int>> pairs;
    for (size_t i = 0; i < bars.size(); i++) {
        for (size_t j = i + 1; j < bars.size(); j++) {
            float lenA = bars[i].length;
            float lenB = bars[j].length;
            float lenRatio = min(lenA, lenB) / max(lenA, lenB);
            float avgLen = (lenA + lenB) / 2;
            float dx = abs(bars[i].rect.center.x - bars[j].rect.center.x);
            float dy = abs(bars[i].rect.center.y - bars[j].rect.center.y);
            float dist = hypot(dx, dy);
            float ratio = dist / avgLen;

            if (bars[i].color != bars[j].color) continue;                           
            if (abs(bars[i].angle - bars[j].angle) > MAX_ANGLE_DIFF) continue;     
            if (lenRatio < MIN_LEN_RATIO) continue;
            if (dy / avgLen > MAX_DY_RATIO) continue;
            if (ratio < MIN_DIST_RATIO || ratio > MAX_DIST_RATIO) continue;

            pairs.push_back({ (int)i, (int)j });               
            cout << "配对: " << i << " 和 " << j << " 间距比=" << ratio << endl;
        }
    }
    cout << "共配出 " << pairs.size() << " 对" << endl;
    return pairs;
}

// 生成装甲板
vector<Armor> buildArmors(const vector<LightBar>& bars, const vector<pair<int, int>>& pairs) {
    vector<Armor> armors;
    for (size_t n = 0; n < pairs.size(); n++) {
        int a = pairs[n].first;
        int b = pairs[n].second;
        if (bars[a].rect.center.x > bars[b].rect.center.x) swap(a, b);  

        cv::Point2f lt, lb, rt, rb;
        BarEnds(bars[a].rect, lt, lb);
        BarEnds(bars[b].rect, rt, rb);

        Armor ar;
        ar.color = bars[a].color;
        ar.corners[0] = lt;                                    // 左上
        ar.corners[1] = rt;                                    // 右上
        ar.corners[2] = rb;                                    // 右下
        ar.corners[3] = lb;                                    // 左下
        ar.center = (lt + rt + rb + lb) * 0.25f;
        vector<cv::Point2f> pts(ar.corners, ar.corners + 4);
        ar.box = cv::boundingRect(pts);
        if (ar.box.height <= 0) continue;                      
        float aspect = (float)ar.box.width / ar.box.height;    
        if (aspect < MIN_ARMOR_ASPECT || aspect > MAX_ARMOR_ASPECT) continue;  
        ar.leftBar = a;                                        
        ar.rightBar = b;                                       
        ar.valid = true;
        armors.push_back(ar);
    }
    return armors;
}

// 在图上画装甲板
void drawArmors(cv::Mat& img, const vector<Armor>& armors) {
    for (size_t n = 0; n < armors.size(); n++) {
        const Armor& ar = armors[n];
        cv::rectangle(img, ar.box, cv::Scalar(0, 255, 0), 1);
        for (int k = 0; k < 4; k++) {
            cv::line(img, ar.corners[k], ar.corners[(k + 1) % 4], cv::Scalar(255, 255, 0), 1);
            cv::circle(img, ar.corners[k], 2, cv::Scalar(0, 0, 255), -1);
            cv::putText(img, to_string(k), ar.corners[k] + cv::Point2f(3, -3),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
        }
        cv::circle(img, ar.center, 3, cv::Scalar(0, 255, 255), -1);
    }
}

// 打印装甲板信息
void printArmors(const vector<Armor>& armors) {
    for (size_t n = 0; n < armors.size(); n++) {
        const Armor& ar = armors[n];
        cout << "装甲板 " << n << " 颜色=" << ar.color
            << " 左灯条=" << ar.leftBar << " 右灯条=" << ar.rightBar
            << " 中心=(" << ar.center.x << "," << ar.center.y << ")"
            << " 外接框=(" << ar.box.x << "," << ar.box.y << ","
            << ar.box.width << "," << ar.box.height << ")"
            << " 有效=" << ar.valid << endl;
        for (int k = 0; k < 4; k++) {
            cout << "  角点" << k << ": (" << ar.corners[k].x << ","
                << ar.corners[k].y << ")" << endl;
        }
    }
}
// 同一块装甲板被重复输出时，只保留第一个
vector<Armor> removeDuplicates(const vector<Armor>& armors) {
    vector<Armor> kept;                                        
    for (size_t i = 0; i < armors.size(); i++) {
        bool dup = false;                                      
        for (size_t j = 0; j < kept.size(); j++) {
            int inter = (armors[i].box & kept[j].box).area();  
            int smaller = min(armors[i].box.area(), kept[j].box.area());   
            if (smaller > 0 && inter > MAX_OVERLAP * smaller) {
                dup = true;                                    
                break;                                         
            }
        }
        if (!dup) kept.push_back(armors[i]);
    }
    return kept;
}

vector<Armor> detectArmors(const cv::Mat& image, DebugInfo* debug = nullptr) {
    vector<Armor> none;                                        
    if (image.empty() || image.channels() != 3) {             
        cerr << "输入图像无效" << endl;
        return none;
    }

    cv::Mat mask = makeMask(image);
    cv::Mat cleanMask = closeMask(mask);
    vector<LightBar> bars = findLightBars(cleanMask, "red");
    vector<pair<int, int>> pairs = matchBars(bars);
    vector<Armor> armors = removeDuplicates(buildArmors(bars, pairs));

    if (debug != nullptr) {                                    
        debug->mask = mask;
        debug->cleanMask = cleanMask;
        debug->candidates = image.clone();
        drawBars(debug->candidates, bars);
    }
    return armors;
}


int main(int argc, char** argv) {
    string path = DEFAULT_IMAGE;                       
    if (argc > 1) path = argv[1];                  

    cv::Mat image = cv::imread(path);

    if (image.empty()) {
        cerr << "图片读取失败，请检查路径: " << path << endl;
        cerr << "当前工作目录: " << filesystem::current_path().string() << endl;   
        return -1;
    }
    cout << "图片: " << path << "  " << image.cols << " x " << image.rows
        << "  通道数: " << image.channels() << endl;

    filesystem::create_directories("output");      

    DebugInfo dbg;                                     
    vector<Armor> armors = detectArmors(image, &dbg);  

    if (armors.empty()) {
        cout << "未检测到装甲板" << endl;
    }
    else {
        printArmors(armors);
    }

    cv::Mat result = image.clone();
    drawArmors(result, armors);
    if (!saveImage("output/mask.png", dbg.mask) ||
        !saveImage("output/cleanMask.png", dbg.cleanMask) ||
        !saveImage("output/candidates.png", dbg.candidates) ||
        !saveImage("output/result.png", result)) {
        return -1;
    }

    cv::imshow("result", result);
    cv::waitKey(0);
    return 0;
}