if (ld.id == 0) 
{ // id 为 0 的层是整个网络的第一层，为网络提供输入数据
    ninputs = netInputLayer->inputsData.size();
    ld.inputBlobsWrappers.resize(ninputs);
    for (size_t i = 0; i < ninputs; i++)
    {
    // 调用 wrap 函数绑定 Mat 对象
    ld.inputBlobsWrappers[i] = wrap(netInputLayer->inputsData[i]);
    } 
}
else
{
    // id > 0 的层的输入绑定
    ld.inputBlobs.resize(ninputs);
    ld.inputBlobsWrappers.resize(ninputs);
    for (size_t i = 0; i < ninputs; i++)
    {
    LayerPin from = ld.inputBlobsId[i];
    CV_Assert(from.valid());
    CV_DbgAssert(layers.count(from.lid) && 
    (int)layers[from.lid].outputBlobs.size() > from.oid);
    ld.inputBlobs[i] = &layers[from.lid].outputBlobs[from.oid];
    // id > 0 的层的输入是前一层的输出，直接复用即可，无须调用 wrap 函数
    ld.inputBlobsWrappers[i] = 
    layers[from.lid].outputBlobsWrappers[from.oid];
 } }

ld.outputBlobsWrappers.resize(ld.outputBlobs.size());
for (int i = 0; i < ld.outputBlobs.size(); ++i)
{
 ld.outputBlobsWrappers[i] = wrap(ld.outputBlobs[i]);
}
ld.internalBlobsWrappers.resize(ld.internals.size());
for (int i = 0; i < ld.internals.size(); ++i)
{
 ld.internalBlobsWrappers[i] = wrap(ld.internals[i]);
}



std::vector<Ptr<BackendWrapper> > blobsWrapper;
if (newWeightAndBias)
{
 Mat wm;
 weightsMat.copyTo(wm); // to handle the case of isContinuous() == false
 wm.reshape(1, blobs[0].dims, blobs[0].size);
 blobsWrapper.push_back(Ptr<BackendWrapper>(new 
VkComBackendWrapper(wm)));
}
else
{
 blobsWrapper.push_back(Ptr<BackendWrapper>(new 
VkComBackendWrapper(blobs[0])));
}
if (has_bias)
{
 Mat biasesMat({out_channel}, CV_32F, &biasvec[0]);
 blobsWrapper.push_back(Ptr<BackendWrapper>(new 
VkComBackendWrapper(biasesMat)));
}