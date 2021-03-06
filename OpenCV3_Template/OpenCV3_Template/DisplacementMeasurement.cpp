#include "pch.h"

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include "opencv2/calib3d/calib3d.hpp"
#include <ctime>


using namespace cv;
using namespace std;

const float calibrationSquareDimension = 0.023;
const Size chessboardDimensions = Size(9, 6);

int const n = 34;
int const m = 45;
int const perimLower = 500;
int const perimUpper = 800;
int const areaLower = 10000;
int const areaUpper = 30000;
double xCo_ref;
double yCo_ref;
double xCo_mark;
double yCo_mark;
double xCo_diff;
double yCo_diff;
double xCo_diff_prev;
double yCo_diff_prev;
double xDisplacement;
double yDisplacement;
double resDisplacement;
int j = 0;
int c = 0;
bool record = false;
bool started = false;

vector<int> resArray(1);
vector<int> xArray(1);
vector<Mat> frameArray(1);

ofstream outStream;



/////////////////////////////////////////////Load Video////////////////////////////////////////////////////

VideoCapture vid("DemoVideo.wmv"); // 0 = first camera connected, 1 = second camera. Input file name for pre recorded video

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mat src, undistorted, morph, grayscale, binary;

RNG rng(12345);

void find_markers(Mat morph, string filename);

void createKnownBoardPosition(Size boardSize, float squareEdgeLength, vector<Point3f>& corners)
{
	for (int i = 0; i < boardSize.height; i++)
	{
		for (int j = 0; j < boardSize.width; j++)
		{
			corners.push_back(Point3f(j*squareEdgeLength, i*squareEdgeLength, 0.0f));

		}
	}
}

void getChessboardCorners(vector<Mat> images, vector<vector<Point2f>>& allFoundCorners, bool showResults = false)
{
	for (vector<Mat>::iterator iter = images.begin(); iter != images.end(); iter++)
	{
		vector<Point2f> pointBuf;
		bool found = findChessboardCorners(*iter, chessboardDimensions, pointBuf, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE | CV_CALIB_CB_FAST_CHECK);

		if (found)
			allFoundCorners.push_back(pointBuf);

		if (showResults)
		{
			drawChessboardCorners(*iter, chessboardDimensions, pointBuf, found);
			imshow("Looking for Corners", *iter);
			waitKey(0);
		}

	}
}

void cameraCalibration(vector<Mat> calibrationImages, Size boardSize, float squareEdgeLength, Mat& cameraMatrix, Mat& distanceCoefficients)
{
	vector<vector<Point2f>> checkerboardImageSpacePoints;
	getChessboardCorners(calibrationImages, checkerboardImageSpacePoints, false);

	vector<vector<Point3f>> worldSpaceCornerPoints(1);
	createKnownBoardPosition(boardSize, squareEdgeLength, worldSpaceCornerPoints[0]);
	worldSpaceCornerPoints.resize(checkerboardImageSpacePoints.size(), worldSpaceCornerPoints[0]);

	vector<Mat> rVectors, tVectors;
	distanceCoefficients = Mat::zeros(8, 1, CV_64F);

	calibrateCamera(worldSpaceCornerPoints, checkerboardImageSpacePoints, boardSize, cameraMatrix, distanceCoefficients, rVectors, tVectors);

	//double totalError = computeReprojectionErrors(worldSpaceCornerPoints, checkerboardImageSpacePoints,rVectors,tVectors,cameraMatrix,distanceCoefficients);
}

bool saveCameraCalibration(string name, Mat cameraMatrix, Mat distanceCoefficients)
{
	ofstream outStream(name);
	if (outStream)
	{
		uint16_t rows = cameraMatrix.rows;
		uint16_t columns = cameraMatrix.cols;

		outStream << rows << endl;
		outStream << columns << endl;

		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < columns; c++)
			{
				double value = cameraMatrix.at<double>(r, c);
				outStream << value << endl;
			}
		}

		rows = distanceCoefficients.rows;
		columns = distanceCoefficients.cols;

		outStream << rows << endl;
		outStream << columns << endl;

		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < columns; c++)
			{
				double value = distanceCoefficients.at<double>(r, c);
				outStream << value << endl;
			}
		}

		outStream.close();
		return true;
	}
	return false;
}

double computeReprojectionErrors(const vector<vector<Point3f> >& objectPoints, const vector<vector<Point2f> >& imagePoints,
	const vector<Mat>& rvecs, const vector<Mat>& tvecs, const Mat& cameraMatrix, const Mat& distCoeffs, vector<float>& perViewErrors)
{
	vector<Point2f> imagePoints2;
	int i, totalPoints = 0;
	double totalErr = 0, err;
	perViewErrors.resize(objectPoints.size());

	for (i = 0; i < (int)objectPoints.size(); ++i)
	{
		projectPoints(Mat(objectPoints[i]), rvecs[i], tvecs[i], cameraMatrix,  // project
			distCoeffs, imagePoints2);
		err = norm(Mat(imagePoints[i]), Mat(imagePoints2), CV_L2);              // difference

		int n = (int)objectPoints[i].size();
		perViewErrors[i] = (float)std::sqrt(err*err / n);                        // save for this view
		totalErr += err * err;                                             // sum it up
		totalPoints += n;
	}

	return std::sqrt(totalErr / totalPoints);              // calculate the arithmetical mean
}

bool loadCameraCalibration(string name, Mat& cameraMatrix, Mat& distanceCoefficients)
{
	ifstream inStream(name);
	if (inStream)
	{
		uint16_t rows;
		uint16_t columns;

		inStream >> rows;
		inStream >> columns;

		cameraMatrix = Mat(Size(columns, rows), CV_64F);

		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < columns; c++)
			{
				double read = 0.0f;
				inStream >> read;
				cameraMatrix.at<double>(r, c) = read;
				//cout << cameraMatrix.at<double>(r, c) << "\n";
			}
		}

		inStream >> rows;
		inStream >> columns;

		distanceCoefficients = Mat::zeros(rows, columns, CV_64F);

		for (int r = 0; r < rows; r++)
		{
			for (int c = 0; c < columns; c++)
			{
				double read = 0.0f;
				inStream >> read;
				distanceCoefficients.at<double>(r, c) = read;
				//cout << distanceCoefficients.at<double>(r, c) << "\n";
			}
		}
		inStream.close();
		return true;
	}
	return false;
}

void cameraCalibrationProcess(Mat& cameraMatrix, Mat& distanceCoefficients)
{
	Mat	frame, drawToFrame;
	vector<Mat> savedImages;
	vector<vector<Point2f>> markerCorners, rejectedCandidates;

	//check for video
	if (!vid.isOpened())
	{
		cout << "No Video";
		return;
	}

	namedWindow("Webcam", CV_WINDOW_AUTOSIZE);

	while (vid.read(frame))
	{
		vector<Vec2f> foundPoints;
		bool found = false;

		found = findChessboardCorners(frame, chessboardDimensions, foundPoints, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
		frame.copyTo(drawToFrame);
		drawChessboardCorners(drawToFrame, chessboardDimensions, foundPoints, found);
		if (found)
			imshow("Webcam", drawToFrame);
		else
			imshow("Webcam", frame);

		char character = waitKey(1000 / 20);

		switch (character)
		{
		case' ':
			//saving image
			if (found)
			{
				Mat temp;
				frame.copyTo(temp);
				savedImages.push_back(temp);
				cout << "1";
			}
			break;
		case 13:
			//start calibration
			if (savedImages.size() > 15)
			{
				cameraCalibration(savedImages, chessboardDimensions, calibrationSquareDimension, cameraMatrix, distanceCoefficients);
				saveCameraCalibration("Camera Calibration", cameraMatrix, distanceCoefficients);
			}
			break;
		case 27:
			//exit
			return;
			break;
		}
	}
}

int main(int argc, char** argv)
{
	char stime[9];
	_strtime_s(stime);
	string filename = "Recorded Values_";
	filename.append(stime);
	filename.append(".txt");
	for (int i = 0; i < filename.length(); ++i)
	{
		if (filename[i] == '/' || filename[i] == ':')
			filename[i] = '-';
	}

	Mat distanceCoefficients, map1, map2;
	Mat cameraMatrix = Mat::eye(3, 3, CV_64F);
	Size imageSize = Size(src.cols, src.rows);

	//cameraCalibrationProcess(cameraMatrix, distanceCoefficients);
	loadCameraCalibration("Camera Calibration", cameraMatrix, distanceCoefficients);

	//check for video
	if (!vid.isOpened())
	{
		cout << "No Video";
		return -1;
	}

	while (vid.read(src))
	{
		initUndistortRectifyMap(cameraMatrix, distanceCoefficients, Mat(),
			getOptimalNewCameraMatrix(cameraMatrix, distanceCoefficients, src.size(), 1, src.size(), 0),
			src.size(), CV_16SC2, map1, map2);

		remap(src, undistorted, map1, map2, INTER_LINEAR);

		cvtColor(undistorted, grayscale, CV_RGB2GRAY);		             //convert image to grayscale
		threshold(grayscale, binary, 50, 255, THRESH_BINARY);			 //Convert pixels above the threshold to white and everything below to black

		//Closed morphology transform is applied using a rectangular kernel 
		Mat element = getStructuringElement(MORPH_RECT, Size(n, m));
		morphologyEx(binary, morph, MORPH_CLOSE, element);


		//create windows for videos
		namedWindow("Source", WINDOW_FREERATIO);
		resizeWindow("Source", src.cols, src.rows);
		namedWindow("Binarized", WINDOW_FREERATIO);
		resizeWindow("Binarized", src.cols, src.rows);
		namedWindow("BinarizedMorph", WINDOW_FREERATIO);
		resizeWindow("BinarizedMorph", src.cols, src.rows);
		imshow("Source", undistorted);									//show Undistorted image
		imshow("Binarized", binary);									//show binarized image
		imshow("BinarizedMorph", morph);								//show binarized image after morphology transform


		// call the find_contours function -------------------------------------------------------------------------------------------------------------------------------------------

		//if (record == true)						**uncomment this line to enable the spacebar keyboard command to stop and start recording (commented out for the demo video)**
			find_markers(morph, filename);

		//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

		//waitKey();
		char character = waitKey(1000 / 200);

		switch (character)
		{
		case' ':
			if (record == false)
			{
				if (started == false)
					outStream.open(filename.c_str());
				if (outStream.fail())
					perror(filename.c_str());
				outStream << "Begin Recording:" << endl;
				record = true;
				started = true;
			}
			else
			{
				outStream << endl;
				record = false;
			}
			break;

		case 27:
			//exit
			return 0;
			break;
		}

	}

	
	namedWindow("Array (Press Spacebar to move to next frame)", WINDOW_FREERATIO);
	resizeWindow("Array (Press Spacebar to move to next frame)", 500, 350);

	for (int i = 0; i < resArray.size()-1; i++)
	{
		cout << resArray[i] << "\t";
		imshow("Array (Press Spacebar to move to next frame)", frameArray[i]);
		waitKey(0);
	}
	
	outStream.close();
	return(0);
}

void find_markers(Mat morph, string filename)
{

	Mat canny_output;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	/// Detect edges using canny edge detection
	Canny(morph, canny_output, 100, 150, 5);
	// Find contours
	findContours(canny_output, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));

	/// Get the moments
	vector<Moments> mu(contours.size());

	double perimeter = 0;
	double area = 0;
	for (int i = 0; i < contours.size(); i++)
	{
		perimeter = arcLength(Mat(contours[i]), true);
		area = contourArea(Mat(contours[i]), false);
		if (perimeter > perimLower && perimeter < perimUpper)
		{
			if (area > areaLower && area < areaUpper)
			{
				mu[i] = moments(contours[i], false);
			}
		}

	}

	vector<Point2f> mc(contours.size());
	int k = 0;
	int objectCount = 0;
	for (int i = 0; i < contours.size(); i++)
	{
		perimeter = arcLength(Mat(contours[i]), true);								//get arclength of the contour
		area = contourArea(Mat(contours[i]), false);								//get the area of the contour

		//Check whether permimeter and area of countour is within set range. This also removes all unclosed contours (area = 0)
		if (perimeter > perimLower && perimeter < perimUpper)
		{
			if (area > areaLower && area < areaUpper)
			{
				if (i % 2 == 0)
				{
					objectCount++; //count the number of countours found within the range
				}
			}
		}
	}


	//Get the centroid of the markers if there are exactly 2 objects found (the 2 markers).
	if (objectCount == 2)
	{
		for (int i = 0; i < contours.size(); i++)
		{
			perimeter = arcLength(Mat(contours[i]), true);
			area = contourArea(Mat(contours[i]), false);
			if (perimeter > perimLower && perimeter < perimUpper)
			{
				if (area > areaLower && area < areaUpper)
				{
					mc[i] = Point2f(mu[i].m10 / mu[i].m00, mu[i].m01 / mu[i].m00);			//get x and y co-ordinates of the centroid
					if (i % 2 == 0)
					{
						//set x and y co-ord for first marker
						if (k == 0)
						{
							xCo_mark = mu[i].m10 / mu[i].m00;
							yCo_mark = mu[i].m01 / mu[i].m00;
						}
						//set x and y co-ord for second marker and calculate the displacement
						if (k == 1)
						{
							xCo_ref = mu[i].m10 / mu[i].m00;
							yCo_ref = mu[i].m01 / mu[i].m00;

							//save the x and y distances between the 2 markers  for the first frame
							if (j == 0)
							{
								cout << "Res:" << resDisplacement;
								cout << "\tX:" << xDisplacement << "\n";

								outStream << "Res:" << resDisplacement << "\tX:" << xDisplacement << endl;

								yCo_diff = yCo_ref - yCo_mark;
								if (yCo_diff < 0)
									yCo_diff = yCo_mark - yCo_ref;
								yCo_diff_prev = yCo_diff;

								xCo_diff = xCo_ref - xCo_mark;
								if (yCo_diff < 0)
									xCo_diff = xCo_mark - xCo_ref;
								xCo_diff_prev = xCo_diff;

								resArray[0] = 0;
								xArray[0] = 0;
								vid >> frameArray[0];
								c++;
							}
							//calculate resultant displacement from starting point in the direction of the reference marker as well as x- and y-axis displacement
							else
							{
								//calcuate diference in y co-ord between the markers and add to y displacement
								yCo_diff = yCo_ref - yCo_mark;
								if (yCo_diff < 0)
									yCo_diff = yCo_mark - yCo_ref;
								yDisplacement += yCo_diff_prev - yCo_diff;

								//calcuate diference in x co-ord between the markers and add to x displacement
								xCo_diff = xCo_ref - xCo_mark;
								if (yCo_diff < 0)
									xCo_diff = xCo_mark - xCo_ref;
								xDisplacement += xCo_diff_prev - xCo_diff;

								resDisplacement = sqrt(pow(xDisplacement, 2) + pow(yDisplacement, 2));		//calculate resultant displacement

								resArray.resize(c + 1);
								resArray[c] = resDisplacement;
								xArray.resize(c + 1);
								xArray[c] = xDisplacement;
								frameArray.resize(c + 1);
								vid >> frameArray[c];

								cout << "Res:" << resArray[c];
								cout << "\tX:" << xDisplacement << "\n";
								outStream << "Res:" << resArray[c] << "\tX:" << xDisplacement << endl;

								//save the x and y distances between the 2 markers 
								xCo_diff_prev = xCo_diff;
								yCo_diff_prev = yCo_diff;
								c++;
							}
							j++;
						}
						k++;
					}

				}
			}

		}
	}


	//Draw contours
	Mat drawing(canny_output.size(), CV_8UC3, Scalar(255, 255, 255));
	if (objectCount == 2)
	{
		for (int i = 0; i < contours.size(); i++)
		{
			perimeter = arcLength(Mat(contours[i]), true);
			area = contourArea(Mat(contours[i]), false);
			if (perimeter > perimLower && perimeter < perimUpper)
			{
				if (area > areaLower && area < areaUpper)
				{
					Scalar color = Scalar(100, 100, 0);
					drawContours(drawing, contours, i, color, 2, 8, hierarchy, 0, Point());
					circle(drawing, mc[i], 4, color, -1, 7, 0);
				}
			}
		}
	}

	/// Show the resultant image
	namedWindow("Contours", WINDOW_FREERATIO);
	resizeWindow("Contours", drawing.cols, drawing.rows);
	imshow("Contours", drawing);
	//waitKey();
}