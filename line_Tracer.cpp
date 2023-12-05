#include <iostream>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include "opencv2/opencv.hpp"
#include "dxl.hpp"
using namespace std;
using namespace cv;
bool ctrl_c_pressed = false;
void ctrlc_handler(int){ ctrl_c_pressed = true; }

int main() {
    string src = "nvarguscamerasrc sensor-id=0 ! video/x-raw(memory:NVMM), width=(int)640, height=(int)360, format=(string)NV12 ! nvvidconv flip-method=0 ! video/x-raw, width=(int)640, height=(int)360, format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! appsink"; 
    
    //string src = "2_lt_ccw_50rpm_out.mp4";
    
    VideoCapture cap(src,CAP_GSTREAMER);
    if (!cap.isOpened()){ cout << "Camera error" << endl; return -1; }
    
    string dst1 = "appsrc ! videoconvert ! video/x-raw, format=BGRx ! nvvidconv ! nvv4l2h264enc insert-sps-pps=true ! h264parse ! rtph264pay pt=96 ! udpsink host=203.234.58.168 port=8001 sync=false";
    string dst2 = "appsrc ! videoconvert ! video/x-raw, format=BGRx ! nvvidconv ! nvv4l2h264enc insert-sps-pps=true ! h264parse ! rtph264pay pt=96 ! udpsink host=203.234.58.168 port=8002 sync=false";
    
    VideoWriter writer1(dst1, 0, (double)30, Size(640, 360), true);
    if (!writer1.isOpened()) { cerr << "Writer open failed!" << endl; return -1;}

    VideoWriter writer2(dst2, 0, (double)30, Size(640, 90), true);
    if (!writer2.isOpened()) { cerr << "Writer open failed!" << endl; return -1;}

    Mat frame, gray, subImage;
    Mat labels, stats, centroids;
    
    int vel1 = 0,vel2 = 0,error=0;
    Dxl mx;
    
    signal(SIGINT, ctrlc_handler);
    if(!mx.open()) { cout << "dynamixel open error"<<endl; return -1; }
    
    while (true) {
        double start = getTickCount();
        //cap의 영상을 frame에 삽입
        cap >> frame;
        if(!cap.read(frame)){ cap.set(CAP_PROP_POS_FRAMES,0); continue;}

        cvtColor(frame, gray, COLOR_BGR2GRAY);
        // 크기 조절한 로드 맵
        subImage = gray(Rect(0, 270, 640, 90));
        GaussianBlur(subImage, subImage, Size(5, 5), 0, 0);
        // 밝기 보정
        double desiredMean = 70;
        Scalar meanValue = mean(subImage);
        double b = desiredMean - meanValue.val[0];
        subImage = subImage + b;
        subImage = max(0, min(255, subImage));

        // 이진화
        threshold(subImage, subImage, 128, 255, THRESH_BINARY);

        int p_width = subImage.cols;
        int p_height = subImage.rows;
        // 레이블링        
        int numLabels = connectedComponentsWithStats(subImage, labels, stats, centroids);
        cvtColor(subImage, subImage, COLOR_GRAY2BGR);
        // 주 로드를 잡을 기준점
        static Point pos((p_width) / 2, p_height / 2);
        circle(subImage, pos, 3, Scalar(255,0,0) ,-1);

        Point integer;
        int error_x = p_width/2; //가장 직선일때의 선의 위치

        // 각각의 좌표점과 박스 색깔
        for (int i = 1; i < numLabels; i++) {
            int left = stats.at<int>(i, CC_STAT_LEFT);
            int top = stats.at<int>(i, CC_STAT_TOP);
            int width = stats.at<int>(i, CC_STAT_WIDTH);
            int height = stats.at<int>(i, CC_STAT_HEIGHT);
            // 객체의 중심점
            double x = centroids.at<double>(i, 0);
            double y = centroids.at<double>(i, 1);
            // 레이블 기준으로 객체 구분 (작은 픽셀들은 노이즈로 취급)
            int label = (stats.at<int>(i, CC_STAT_AREA));
            if (label < 100)continue;

            integer = pos - Point(x, y);
            if ((integer.x > 0 && integer.x <= 100) || (integer.x <= 0 && integer.x >= -100)){
                pos = Point(x, y);
                // 사각형            
                rectangle(subImage, Point(left, top), Point(left + width, top + height), Scalar(0, 255, 0), 2);
                 // 원 (중심 좌표에 점 찍기)
                circle(subImage, Point(static_cast<int>(x), static_cast<int>(y)), 3, Scalar(0, 0, 255), -1);
            }
            else{
                // 사각형            
                rectangle(subImage, Point(left, top), Point(left + width, top + height), Scalar(0, 0, 255), 2);
                // 원 (중심 좌표에 점 찍기)
                circle(subImage, Point(static_cast<int>(x), static_cast<int>(y)), 3, Scalar(255, 0, 0), -1);
            }
        }
        writer1 << frame;
        writer2 << subImage;
        error = error_x - pos.x;
        vel1 = 100 - 0.22* error;
        vel2 = -(100 + 0.22* error);
        mx.setVelocity(vel1,vel2);
        if (ctrl_c_pressed) break;
        waitKey(30);
        // 종료 시간 기록
        double end = getTickCount();
        // 실행 시간 계산 (초 단위로 변환)
        double elapsedTime = (end - start) / getTickFrequency();
        // 결과 출력
        cout << "vel1: " << vel1 << ", vel2: " << vel2 <<", error: " << error << " 실행 시간: " << elapsedTime << " 초" << endl;
    }
    // 영상의 재생이 끝나면 종료
    mx.close();
    return 0;
}