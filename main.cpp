#include<iostream>
#include<vector>
#include<string>
#include<cmath>
#include<math.h>
#include<fstream>

#include"opencv2\opencv.hpp"
#include"opencv2\core.hpp"
#include"opencv2\imgproc.hpp"
#include"opencv2\xfeatures2d.hpp"
#include"opencv2\flann.hpp"

#include"Transformations.h"
const cv::Size GAUSSIAN_SMOOTH_FILTER_SIZE = cv::Size(3, 3);
const int ADAPTIVE_THRESH_BLOCK_SIZE = 15;
const int ADAPTIVE_THRESH_WEIGHT = 11;

// file with filenames of templates
#define TEMPATES_FILENAME "listCurrencyDB_u.txt"
#define TEMPLATES_DIR "currencyDB_lowResolution_m\\"
#define EXTENSION ".jpg"
#define MASK_TOKEN "-mask"

float angleBetween(const cv::Point2f& p1, const cv::Point2f& p2, const cv::Point2f& p3)
{
	// Compute lengths
	double len1 = sqrt((p2.x - p1.x)*(p2.x - p1.x) + (p2.y - p1.y)*(p2.y - p1.y));
	double len2 = sqrt((p3.x - p2.x)*(p3.x - p2.x) + (p3.y - p2.y)*(p3.y - p2.y));

	// Compute dots
	double dot = sqrt(fabs((p2.x - p1.x)*(p3.x - p2.x)) + fabs((p2.y - p1.y)*(p3.y - p2.y)));

	// Compute cos
	double a = dot / (len1 * len2);
	// Return
	return a;
}

bool verifyRectangle(const cv::Mat& image, const cv::Point2f& p1, const cv::Point2f& p2, const cv::Point2f& p3, const cv::Point2f& p4)
{
	// Verify points
	if (p1.x >= 0 && p1.x < image.cols && p1.y >= 0 && p1.y < image.rows && p2.x >= 0 && p2.x < image.cols && p2.y >= 0 && p2.y < image.cols
		&& p3.x >= 0 && p3.x < image.cols && p3.y >= 0 && p3.y < image.cols && p4.x >= 0 && p4.x < image.cols && p4.y >= 0 && p4.y < image.cols) {
		if ((fabs(angleBetween(p1, p2, p3)) < 0.01) && (fabs(angleBetween(p2, p3, p4)) < 0.01) && (fabs(angleBetween(p3, p4, p1)) < 0.01) && (fabs(angleBetween(p4, p1, p2)) < 0.01))
		{
			std::vector<cv::Point> cont = { p1, p2, p3, p4 };
			if (cv::contourArea(cont) > 0.01 * image.cols * image.rows)
			{
				// check if diagonal is bigger than sides
				double a = sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));
				double b = sqrt((p3.x - p2.x) * (p3.x - p2.x) + (p3.y - p2.y) * (p3.y - p2.y));
				double c = sqrt((p3.x - p1.x) * (p3.x - p1.x) + (p3.y - p1.y) * (p3.y - p1.y));
				if (c > b && c > a) {
					return true;
				}
				else {
					return false;
				}
				
			}
			else {
				return false;
			}
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}


bool isInImage(const cv::Mat& image, const cv::Point2f& p1, const cv::Point2f& p2, const cv::Point2f& p3, const cv::Point2f& p4)
{
	if (p1.x >= 0 && p1.x < image.cols && p1.y >= 0 && p1.y < image.rows && p2.x >= 0 && p2.x < image.cols && p2.y >= 0 && p2.y < image.cols
		&& p3.x >= 0 && p3.x < image.cols && p3.y >= 0 && p3.y < image.cols && p4.x >= 0 && p4.x < image.cols && p4.y >= 0 && p4.y < image.cols)
	{
		return true;
	}
	else {
		return false;
	}

}

// Function for extracting cash
std::vector<cv::Mat> extractCash(const cv::Mat& img)
{
	cv::Mat gray;
	cv::cvtColor(img, gray, CV_BGR2GRAY);

	// Calculate gradients gx, gy
	cv::Mat gx, gy;
	Sobel(gray, gx, CV_32F, 1, 0, 1);
	Sobel(gray, gy, CV_32F, 0, 1, 1);

	cv::Mat gxThresh, gyThresh;

	cv::threshold(gx, gxThresh, -6.5, 128, CV_THRESH_BINARY_INV);
	cv::threshold(gy, gyThresh, -6.5, 127, CV_THRESH_BINARY_INV);

	// Compute gradient
	cv::Mat grad;
	cv::add(gxThresh, gyThresh, grad);
	cv::convertScaleAbs(grad, grad);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(grad, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	// Get convex hull
	std::vector<std::vector<cv::Point> >hull(contours.size());

	// Verify hull contours
	std::vector<std::vector<cv::Point>> hullVerified;
	for (int i = 0; i < contours.size(); i++)
	{
		convexHull(cv::Mat(contours[i]), hull[i], false);
		if (cv::contourArea(hull[i]) > 50)
			hullVerified.push_back(hull[i]);
	}

	cv::Mat blank(img.size(), CV_8U, cv::Scalar(0));

	cv::drawContours(blank, hullVerified, -1, cv::Scalar(255), -1);

	// Erode blank
	cv::erode(blank, blank, cv::Mat(3, 3, CV_8U, cv::Scalar::all(1)), cv::Point(-1, -1));

	// Make mask bigger
	cv::dilate(blank, blank, cv::Mat(8, 8, CV_8U, cv::Scalar::all(1)), cv::Point(-1, -1), 5);

	// Find contours in blank
	std::vector<std::vector<cv::Point>> contoursBlank;
	cv::findContours(blank, contoursBlank, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	// Get bounding rectangle
	std::vector<std::vector<cv::Point> > contours_poly(contoursBlank.size());
	std::vector<cv::Rect> boundRect(contoursBlank.size());

	// Image for drawing rectangles
	cv::Mat drawing(img.size(), CV_8U, cv::Scalar::all(0));

	// Get rectangles + find mean area of rectangle
	double meanArea = 0;
	for (int i = 0; i < contoursBlank.size(); i++)
	{
		approxPolyDP(cv::Mat(contoursBlank[i]), contours_poly[i], 3, true);
		boundRect[i] = boundingRect(cv::Mat(contours_poly[i]));
		meanArea += boundRect[i].width * boundRect[i].height;
	}
	meanArea /= contoursBlank.size();

	// Get rectangles which is bigger than mean area
	std::vector<cv::Mat> cashes;

	for (int i = 0; i < contoursBlank.size(); ++i)
	{
		if (boundRect[i].width * boundRect[i].height > meanArea)
		{
			cv::Mat temp = img(boundRect[i]);
			cashes.push_back(temp);
		}
	}

	return cashes;
}

cv::Mat extractValue(cv::Mat &imgOriginal) {
	cv::Mat imgHSV;
	std::vector<cv::Mat> vectorOfHSVImages;
	cv::Mat imgValue;

	cv::cvtColor(imgOriginal, imgHSV, CV_BGR2HSV);

	cv::split(imgHSV, vectorOfHSVImages);

	imgValue = vectorOfHSVImages[2];

	return(imgValue);
}


cv::Mat maximizeContrast(cv::Mat &imgGrayscale) {
	cv::Mat imgTopHat;
	cv::Mat imgBlackHat;
	cv::Mat imgGrayscalePlusTopHat;
	cv::Mat imgGrayscalePlusTopHatMinusBlackHat;

	cv::Mat structuringElement = cv::getStructuringElement(CV_SHAPE_RECT, cv::Size(3, 3));

	cv::morphologyEx(imgGrayscale, imgTopHat, CV_MOP_TOPHAT, structuringElement);
	cv::morphologyEx(imgGrayscale, imgBlackHat, CV_MOP_BLACKHAT, structuringElement);

	imgGrayscalePlusTopHat = imgGrayscale + imgTopHat;
	imgGrayscalePlusTopHatMinusBlackHat = imgGrayscalePlusTopHat - imgBlackHat;

	return(imgGrayscalePlusTopHatMinusBlackHat);
}


std::vector<cv::Rect> preprocessImageAndGetRectangles(cv::Mat& image)
{
	// extract value channel only from original image to get imgGrayscale
	cv::Mat gray = extractValue(image);

	// maximize contrast with top hat and black hat
	cv::Mat imgMaxContrastGrayscale = maximizeContrast(gray);

	cv::Mat imgBlurred;

	// Create size for Gaussian blur

	// gaussian blur
	cv::GaussianBlur(imgMaxContrastGrayscale, imgBlurred, GAUSSIAN_SMOOTH_FILTER_SIZE, 0);

	// call adaptive threshold to get imgThresh
	cv::Mat thresh;
	cv::adaptiveThreshold(imgBlurred, thresh, 255.0, CV_ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY_INV, ADAPTIVE_THRESH_BLOCK_SIZE * 2 + 1, ADAPTIVE_THRESH_WEIGHT);

	cv::dilate(thresh, thresh, cv::Mat(11, 11, CV_8U, cv::Scalar::all(1)));
	//cv::morphologyEx(thresh, thresh, CV_MOP_CLOSE, cv::Mat(3, 3, CV_8U, cv::Scalar::all(1)));

	// Find contours at blank image
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(thresh, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	// Get mean contour area

	// Get rectangles around contours, which area > meanContourArea
	std::vector<std::vector<cv::Point> > contours_poly(contours.size());
	std::vector<cv::Rect> boundRect(contours.size());
	for (int i = 0; i < contours.size(); i++)
	{
		approxPolyDP(cv::Mat(contours[i]), contours_poly[i], 3, true);
		boundRect[i] = boundingRect(cv::Mat(contours_poly[i]));
	}

	// Find meanRectArea
	double meanRectArea = 0;
	for (int i = 0; i < boundRect.size(); ++i)
	{
		meanRectArea += boundRect[i].width * boundRect[i].height;
	}
	meanRectArea /= boundRect.size();

	if (meanRectArea < image.cols * image.rows * 0.01)
	{
		meanRectArea = image.cols * image.rows * 0.01;
	}

	// Verify rectangles
	std::vector<cv::Rect> verifiedRect;
	for (int i = 0; i < boundRect.size(); ++i)
	{
		if ((boundRect[i].width * boundRect[i].height) > meanRectArea)
		{
			verifiedRect.push_back(boundRect[i]);
		}
	}

	// Return result
	return verifiedRect;
}


std::vector<cv::Mat> getTemplates()
{
	/* 
	*  Function reads templates filename 
	*  then reads all images in templates directory
	*  copy them with mask
	*  push back into result vector
	*  in case templates filename wasn`t opened or dir was empty 
	*  return empty vector
	*  result vector
	*/
	std::vector<cv::Mat> templates;
	// open filename with templates filenames
	std::ifstream fin(TEMPATES_FILENAME);
	if (fin.is_open())
	{
		std::string line;
		while (!fin.eof())
		{
			// Get filename without extension
			getline(fin, line);
			// load image
			cv::Mat temp = cv::imread(TEMPLATES_DIR + line + EXTENSION);
			templates.push_back(temp);
		}
	}
	return templates;
}

int main()
{
	cv::Mat image = cv::imread("TestDB\\1-1-100-200-500.jpg");

	std::vector<cv::Rect> rectangles = preprocessImageAndGetRectangles(image);

	std::cout << rectangles.size() << std::endl;

	// Detect keypoints with SIFT
	cv::Ptr<cv::xfeatures2d::SIFT> detector = cv::xfeatures2d::SIFT::create();

	// get templates
	std::vector<cv::Mat> templates = getTemplates();

	// Extract images with rectangles
	std::vector<cv::Mat> cashImages;
	for (int i = 0; i < rectangles.size(); ++i)
	{
		// extract image
		cv::Mat temp = image(rectangles[i]);
		// Get keypoints adn descriptors for temp image - extracted with rectangle
		std::vector<cv::KeyPoint> keypoint;
		cv::Mat descriptors;
		detector->detectAndCompute(temp, cv::Mat(), keypoint, descriptors);
		
		int bfNormType = cv::NORM_L2;
		cv::BFMatcher matcher(bfNormType);

		// Go throw all templates
		for (int k = 0; k < templates.size(); ++k)
		{
			if (!templates[k].empty()) {
				double max_dist = 0; double min_dist = 100;

				// Get keypoints and descriptors for template
				std::vector<cv::KeyPoint> keypointTemplate;
				cv::Mat descriptorsTemplate;

				detector->detectAndCompute(templates[k], cv::Mat(), keypointTemplate, descriptorsTemplate);

				// Check if there are keypoints and descriptors
				if (!descriptorsTemplate.empty() && !keypointTemplate.empty() && !descriptors.empty()) {

					// Get matches
					std::vector< cv::DMatch > matches;
					matcher.match(descriptorsTemplate, descriptors, matches);

					// Check if there are some matches
					if (matches.size() > 0) {

						for (int j = 0; j < descriptorsTemplate.rows; ++j)
						{
							double dist = matches[j].distance;
							if (dist < min_dist) min_dist = dist;
							if (dist > max_dist) max_dist = dist;
						}

						printf("-- Max dist : %f \n", max_dist);
						printf("-- Min dist : %f \n", min_dist);

						// Get only "good" matches (i.e. whose distance is less than 3*min_dist )
						std::vector< cv::DMatch > good_matches;
						for (int m = 0; m < descriptorsTemplate.rows; ++m)
						{
							if (matches[m].distance <= 3 * min_dist)
							{
								good_matches.push_back(matches[m]);
							}
						}

						// Check if there is good matches
						if (good_matches.size() > 0) {
							cv::Mat img_matches;
							drawMatches(templates[k], keypointTemplate, temp, keypoint,
								good_matches, img_matches, cv::Scalar::all(-1), cv::Scalar::all(-1),
								std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
							//-- Localize the object
							std::vector<cv::Point2f> obj;
							std::vector<cv::Point2f> scene;
							for (size_t l = 0; l < good_matches.size(); ++l)
							{
								//-- Get the keypoints from the good matches
								obj.push_back(keypointTemplate[good_matches[l].queryIdx].pt);
								scene.push_back(keypoint[good_matches[l].trainIdx].pt);
							}

							// Cheack if obj and scene is not empty
							if (obj.size() >= 4 && scene.size() >= 4) {
								cv::Mat H = Transformations::findHomography(obj, scene, cv::RANSAC);

								if (!H.empty()) {
									//-- Get the corners from the image_1 ( the object to be "detected" )
									std::vector<cv::Point2f> obj_corners(4);
									obj_corners[0] = cvPoint(0, 0); obj_corners[1] = cvPoint(templates[k].cols, 0);
									obj_corners[2] = cvPoint(templates[k].cols, templates[k].rows); obj_corners[3] = cvPoint(0, templates[k].rows);
									std::vector<cv::Point2f> scene_corners(4);
									perspectiveTransform(obj_corners, scene_corners, H);
									if (verifyRectangle(image, scene_corners[0] + cv::Point2f(rectangles[i].tl()), scene_corners[1] + cv::Point2f(rectangles[i].tl()), scene_corners[2] + cv::Point2f(rectangles[i].tl()), scene_corners[3] + cv::Point2f(rectangles[i].tl()))) {
										//-- Draw lines between the corners (the mapped object in the scene - image_2 )
										line(image, scene_corners[0] + cv::Point2f(rectangles[i].tl()), scene_corners[1] + cv::Point2f(rectangles[i].tl()), Scalar(0, 255, 0), 4);
										line(image, scene_corners[1] + cv::Point2f(rectangles[i].tl()), scene_corners[2] + cv::Point2f(rectangles[i].tl()), Scalar(0, 255, 0), 4);
										line(image, scene_corners[2] + cv::Point2f(rectangles[i].tl()), scene_corners[3] + cv::Point2f(rectangles[i].tl()), Scalar(0, 255, 0), 4);
										line(image, scene_corners[3] + cv::Point2f(rectangles[i].tl()), scene_corners[0] + cv::Point2f(rectangles[i].tl()), Scalar(0, 255, 0), 4);

										//line(img_matches, scene_corners[0] + cv::Point2f(templates[k].cols, 0), scene_corners[1] + cv::Point2f(templates[k].cols, 0), Scalar(0, 255, 0), 4);
										//line(img_matches, scene_corners[1] + cv::Point2f(templates[k].cols, 0), scene_corners[2] + cv::Point2f(templates[k].cols, 0), Scalar(0, 255, 0), 4);
										//line(img_matches, scene_corners[2] + cv::Point2f(templates[k].cols, 0), scene_corners[3] +  cv::Point2f(templates[k].cols, 0), Scalar(0, 255, 0), 4);
										//line(img_matches, scene_corners[3] + cv::Point2f(templates[k].cols, 0), scene_corners[0] + cv::Point2f(templates[k].cols, 0), Scalar(0, 255, 0), 4);
										imshow("Good Matches & Object detection", image);
										cv::waitKey(0);
									}// chaeck if rectangle vefiried
								}// check if H is empty
							}// check if obj and scene is not empty
						}// chaeck if there is good points
					} // check if there are some matches
				}// check if there are keypoints and descriptors
			}// check if template image is empty
		}
		
	}	
 	return 0;
}
 