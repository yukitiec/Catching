#pragma once

#ifndef SEQUENCE_H
#define SEQUENCE_H

#include "stdafx.h";

// 3D positioning ~ trajectory prediction
extern std::queue<int> queueTargetFrameIndex;                      // TM estimation frame
extern std::queue<std::vector<cv::Rect2d>> queueTargetBboxesLeft;  // bboxes from template matching for predict objects' trajectory
extern std::queue<std::vector<cv::Rect2d>> queueTargetBboxesRight; // bboxes from template matching for predict objects' trajectory
extern std::queue<std::vector<int>> queueTargetClassIndexesLeft;   // class from template matching for maintain consistency
extern std::queue<std::vector<int>> queueTargetClassIndexesRight;  // class from template matching for maintain consistency

class Sequence
{
public:
	Sequence()
	{
		std::cout << "construct Sequence class" << std::endl;
	}

    void updateData(std::vector<std::vector<std::vector<int>>>& dataLeft, std::vector<std::vector<int>>& classesLeft)
    {
        int frameIndex;
        std::vector<int> classesCurrentLeft; // latest classes
        std::vector<cv::Rect2d> bboxesLeft;  // latest datas
        /* get tracking data fromt TM */
        bool ret = getTargetData(frameIndex, classesCurrentLeft, bboxesLeft);
        /* data available */
        if (ret)
        {
            std::vector<std::vector<int>> dataCurrentLeft;
            convertRoi2Center(classesCurrentLeft, bboxesLeft, frameIndex, dataCurrentLeft); // dataCurrentLeft is latest data : [frameIndex,classLabel,xCenter,yCenter]
            /* compare data with past data */
            /* past data exist */
            if (!dataLeft.empty())
            {
                /* compare class indexes and match which object is the same */
                /* labels should be synchronized with TM labels */
                //std::cout << "update data and save" << std::endl;
                addData(classesLeft, dataLeft, dataCurrentLeft, classesCurrentLeft); // classesLeft and dataLeft is storage, dataCurrentLeft is latest data, and updatedClassesLeft is new class list
            }
            /* past data doesn't exist */
            else
            {
                int counterCurrentTracker = 0;
                std::vector<int> newClasses;
                for (const int& classIndexCurrent : classesCurrentLeft)
                {
                    // detected
                    if (classIndexCurrent != -1)
                    {
                        dataLeft.push_back({ dataCurrentLeft[counterCurrentTracker] });
                        newClasses.push_back(classIndexCurrent); // add new class
                        counterCurrentTracker++;
                    }
                    // not detected
                    else
                    {
                        newClasses.push_back(-1);
                    }
                }
                classesLeft.push_back(newClasses);
            }
        }
        /* data isn't available */
        else
        {
            /* Nothing to do */
        }
    }

    bool getTargetData(int& frameIndex, std::vector<int>& classesLeft, std::vector<cv::Rect2d>& bboxesLeft)
    {
        //std::unique_lock<std::mutex> lock(mtxTarget); // Lock the mutex
        /* both data is available */
        bool ret = false;
        if (!queueTargetBboxesLeft.empty())
        {
            frameIndex = queueTargetFrameIndex.front();
            classesLeft = queueTargetClassIndexesLeft.front();
            bboxesLeft = queueTargetBboxesLeft.front();
            queueTargetFrameIndex.pop();
            queueTargetClassIndexesLeft.pop();
            queueTargetBboxesLeft.pop();
            ret = true;
        }
        return ret;
    }

    void convertRoi2Center(std::vector<int>& classes, std::vector<cv::Rect2d>& bboxes, const int& frameIndex, std::vector<std::vector<int>>& newData)
    {
        int counterRoi = 0;
        for (const int& classIndex : classes)
        {
            /* bbox is available */
            if (classIndex != -1)
            {
                int centerX = (int)(bboxes[counterRoi].x + bboxes[counterRoi].width / 2);
                int centerY = (int)(bboxes[counterRoi].y + bboxes[counterRoi].height / 2);
                newData.push_back({ frameIndex, classIndex, centerX, centerY });
                counterRoi++;
            }
            /* bbox isn't available */
            else
            {
                /* nothing to do */
            }
        }
    }

    void addData(std::vector<std::vector<int>>& classes, std::vector<std::vector<std::vector<int>>>& data,
        std::vector<std::vector<int>>& dataCurrent, std::vector<int>& classesCurrent)
    {
        int counterPastClass = 0;                      // for counting past data
        int counterPastTracker = 0;                    // for counting past class
        int counterCurrentTracker = 0;                 // for counting current data
        std::vector<int> classesPast = classes.back(); // get last data from storage so classesLeft or classesRight : [time_steps, classIndexes]
        int numPastClass = classesPast.size();         // number of classes
        //std::cout << "sequence :: last time, num of class = " << numPastClass << std::endl;
        std::vector<int> newClasses;
        /* check every classes */
        //std::cout << "dataCurrent size=" << dataCurrent.size() << ", data.size()=" << data.size() << std::endl;
        for (const int& classIndexCurrent : classesCurrent)
        {
            //std::cout << "index:" << counterPastClass << ", numPastClass=" << numPastClass << ", classIndex=" << classIndexCurrent << std::endl;
            /* within already classes */
            if (counterPastClass < numPastClass)
            {
                //std::cout << "within same class" << std::endl;
                /* new position data found-> add sequence data */
                if (classIndexCurrent != -1)
                {
                    //std::cout << "add new sequential data" << std::endl;
                    data[counterPastTracker].push_back(dataCurrent[counterCurrentTracker]); // add new data in the back of existed data
                    newClasses.push_back(classIndexCurrent);                            // update class list
                    /* revival */
                    if (classesPast[counterPastClass]==-1) 
                        std::cout << "redetect existed object " << std::endl;
                    counterPastClass++;
                    counterPastTracker++;
                    counterCurrentTracker++;
                }
                /* lost data */
                else
                {
                    /* tracker lost -> delete existed data */
                    if (classesPast[counterPastClass] != -1)
                    {
                        //data.erase(data.begin() + counterPastTracker); // erase data
                        newClasses.push_back(-1);
                        counterPastClass++;
                        counterPastTracker++;
                    }
                    /* already lost */
                    else
                    {
                        newClasses.push_back(-1);
                        counterPastClass++;
                        counterPastTracker++;
                    }
                }
            }
            /* new tracker */
            else
            {
                /* new tracker */
                if (classIndexCurrent != -1)
                {
                    data.push_back({ dataCurrent[counterCurrentTracker] });
                    newClasses.push_back(classIndexCurrent); // add new class
                    counterCurrentTracker++;
                    counterPastClass++;
                    counterPastTracker++;
                }
                /* if new tracker class is -1, this is awkward */
                else
                {
                    //std::cout << "although new tracker, class label is -1. check code and modify" << std::endl;
                    newClasses.push_back(-1);
                    data.push_back({ {-1,-1,-1,-1} });
                    counterPastClass++;
                    counterPastTracker++;
                }
            }
        }
        classes.push_back(newClasses);
    }

};

#endif
