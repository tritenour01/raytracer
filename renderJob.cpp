#include <renderJob.h>

RenderJob::RenderJob(QString name, std::string data, int num, int id)
{
    jobID = id;
    jobName = name;
    nameNum = num;
    status = Waiting;
    progress = 0;
    image = NULL;
    preview = NULL;
    jobData = data;
}

int RenderJob::getID(void)
{
    return jobID;
}

QString RenderJob::getName(void)
{
    return jobName;
}

int RenderJob::getNum(void)
{
    return nameNum;
}

RenderJob::STATUS RenderJob::getStatus(void)
{
    return status;
}

int RenderJob::getProgress(void)
{
    return progress;
}

std::string RenderJob::getData(void)
{
    return jobData;
}

void RenderJob::setStatus(RenderJob::STATUS s)
{
    status = s;
}

void RenderJob::setImage(UIimage* i)
{
    image = i;
}

void RenderJob::showViewer(void)
{
    if(preview == NULL)
        preview = new PreviewWindow(image);
    preview->show();
}

QString RenderJob::statusToString(RenderJob::STATUS s)
{
    switch(s)
    {
        case RenderJob::Waiting:
            return "Waiting";
        case RenderJob::Rendering:
            return "Rendering";
        case RenderJob::Complete:
            return "Complete";
        case RenderJob::Cancelled:
            return "Cancelled";
    }
}
