using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Shapes;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Threading;
using AvaloniaEdit.Document;
using LogicAnalyzer.Classes;
using LogicAnalyzer.Extensions;
using LogicAnalyzer.Interfaces;
using SharedDriver;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;

namespace LogicAnalyzer.Controls
{
    public partial class SampleViewer : UserControl, ISampleDisplay, IRegionDisplay, IMarkerDisplay
    {
        const int MIN_CHANNEL_HEIGHT = 48;

        public int PreSamples { get; set; }
        public int[]? Bursts { get; set; }
        private AnalyzerChannel[]? channels;
        public int VisibleSamples { get; private set; }
        public int FirstSample { get; private set; }
        public int? UserMarker { get; private set; }

        bool updating = false;

        List<SampleRegion> regions = new List<SampleRegion>();
        List<interval[]> intervals = new List<interval[]>();

        public SampleRegion[] Regions { get { return regions.ToArray(); } }

        //List<ProtocolAnalyzedChannel> analysisData = new List<ProtocolAnalyzedChannel>();
        Color sampleLineColor = Color.FromRgb(60, 60, 60);
        Color sampleDashColor = Color.FromArgb(60, 60, 60, 60);
        Color triggerLineColor = Colors.White;
        Color burstLineColor = Colors.Azure;

        public SampleViewer()
        {
            InitializeComponent();
            this.PointerMoved += SampleViewer_PointerMoved; ; 
        }

        private void SampleViewer_PointerMoved(object? sender, PointerEventArgs e)
        {
            if (intervals.Count == 0 || channels == null)
                return;

            double sampleWidth = Bounds.Width / (double)VisibleSamples;
            var curSample = (int)(e.GetPosition(this).X / sampleWidth) + FirstSample;
            var visibleChannels = channels.Where(c => !c.Hidden).ToArray();
            var curChan = (int)(e.GetPosition(this).Y / (Bounds.Height / visibleChannels.Length));

            if (curChan >= visibleChannels.Length)
                return;

            var chan = visibleChannels[curChan];
            int idx = -1;
            for (int buc = 0; buc < channels.Length; buc++)
            {
                if(channels[buc] == chan)
                {
                    idx = buc;
                    break;
                }
            }

            if (idx == -1)
                return;

            var interval = intervals[idx].FirstOrDefault(i => i.start <= curSample && i.end >curSample);

            if (interval != null)
            {
                var text = $"{chan.ToString()}\nState: {(interval.value ? "High" : "Low")}\nLength: {interval.duration.ToSmallTime()} ({interval.end - interval.start} samples)";
                if (ToolTip.GetTip(this)?.ToString() != text || !ToolTip.GetIsOpen(this))
                {

                    ToolTip.SetTip(this, text);
                    ToolTip.SetIsOpen(this, false);
                    ToolTip.SetPlacement(this, PlacementMode.Pointer);
                    ToolTip.SetShowDelay(this, 0);
                    ToolTip.SetIsOpen(this, true);

                }

                return;
            }
            else
            {
                ToolTip.SetIsOpen(this, false);
            }


        }

        public void SetChannels(AnalyzerChannel[]? Channels, int SampleFrequency)
        {
            channels = Channels;

            ComputeIntervals(SampleFrequency);

            this.InvalidateVisual();
        }

        private void ComputeIntervals(int sampleFrequency)
        {
            intervals.Clear();

            if (channels == null)
                return;

            for (int buc = 0; buc < channels.Length; buc++)
            {
                List<interval> chanIntervals = new List<interval>();

                AnalyzerChannel channel = channels[buc];

                if (channel.Samples == null || channel.Samples.Length == 0)
                    continue;

                byte lastSample = channel.Samples[0];
                int lastSampleIndex = 0;
                double lastSampleTime = 0;

                for (int curSample = 1; curSample < channel.Samples.Length; curSample++)
                {
                    byte sample = channel.Samples[curSample];

                    if (sample != lastSample)
                    {
                        interval newInterval = new interval();
                        newInterval.start = lastSampleIndex;
                        newInterval.end = curSample;
                        newInterval.duration = (curSample - lastSampleIndex) / (double)sampleFrequency;
                        newInterval.value = lastSample != 0;

                        chanIntervals.Add(newInterval);

                        lastSample = sample;
                        lastSampleIndex = curSample;
                        lastSampleTime = curSample / (double)sampleFrequency;
                    }
                }

                interval lastInterval = new interval();
                lastInterval.start = lastSampleIndex;
                lastInterval.end = channel.Samples.Length;
                lastInterval.duration = (channel.Samples.Length - lastSampleIndex) / (double)sampleFrequency;
                lastInterval.value = lastSample != 0;

                chanIntervals.Add(lastInterval);

                intervals.Add(chanIntervals.ToArray());

            }

        }

        public void BeginUpdate()
        {
            updating = true;
        }

        public void EndUpdate()
        {
            updating = false;
            this.InvalidateVisual();
        }
        
        #region Interfaces

        public void AddRegion(SampleRegion Region)
        {
            regions.Add(Region);
            this.InvalidateVisual();
        }
        public void AddRegions(IEnumerable<SampleRegion> Regions)
        {
            regions.AddRange(Regions);
            this.InvalidateVisual();
        }
        public bool RemoveRegion(SampleRegion Region)
        {
            var res = regions.Remove(Region);

            if(res)
                this.InvalidateVisual();

            return res;
        }
        public void ClearRegions()
        {
            regions.Clear();
            this.InvalidateVisual();
        }
        public void UpdateVisibleSamples(int FirstSample, int VisibleSamples)
        {
            this.FirstSample = FirstSample;
            this.VisibleSamples = VisibleSamples;

            if(!updating)
                this.InvalidateVisual();
        }
        public void SetUserMarker(int? UserMarker)
        {
            this.UserMarker = UserMarker;

            if (!updating)
                this.InvalidateVisual();
        }
        #endregion

        public override void Render(DrawingContext context)
        {

            if (channels == null || channels.Length == 0 || channels[0].Samples == null || channels[0].Samples.Length == 0)
                return;

            var visibleChannels = channels.Where(c => !c.Hidden).ToArray();

            int ChannelCount = visibleChannels.Length;

            if(ChannelCount == 0)
                return;

            int minSize = ChannelCount * MIN_CHANNEL_HEIGHT;

            if (this.MinHeight != minSize)
            {
                Dispatcher.UIThread.Post(() =>
                {
                    this.MinHeight = minSize;
                });
            }

            base.Render(context);
            Rect thisBounds = new Rect(0, 0, Bounds.Width, Bounds.Height);
            using (context.PushClip(thisBounds))
            {
                if (VisibleSamples == 0 || updating)
                    return;

                double channelHeight = thisBounds.Height / (double)ChannelCount;
                double sampleWidth = thisBounds.Width / (double)VisibleSamples;
                double margin = channelHeight / 5;

                int lastSample = Math.Min(VisibleSamples + FirstSample, visibleChannels[0].Samples.Length);

                
                for (int chan = 0; chan < ChannelCount; chan++)
                {
                    context.FillRectangle(GraphicObjectsCache.GetBrush(AnalyzerColors.BgChannelColors[chan % 2]), new Rect(0, chan * channelHeight, thisBounds.Width, channelHeight));
                }
                
                if (regions.Count > 0)
                {
                    foreach (var region in regions)
                    {
                        int first = Math.Min(region.FirstSample, region.LastSample);
                        double start = (first - FirstSample) * sampleWidth;
                        double end = sampleWidth * region.SampleCount;
                        context.FillRectangle(GraphicObjectsCache.GetBrush(region.RegionColor), new Rect(start, 0, end, this.Bounds.Height));
                    }
                }

                channelRenderStatus[] renders = new channelRenderStatus[ChannelCount];

                for (int curSample = FirstSample; curSample < lastSample; curSample++)
                {
                    double lineX = (curSample - FirstSample) * sampleWidth;

                    if (VisibleSamples < 201)
                    {
                        context.DrawLine(GraphicObjectsCache.GetPen(sampleLineColor, 1), new Point(lineX + sampleWidth / 2, 0), new Point(lineX + sampleWidth / 2, thisBounds.Height));

                        if (VisibleSamples < 101)
                        {
                            context.DrawLine(GraphicObjectsCache.GetPen(sampleDashColor, 1), new Point(lineX, 0), new Point(lineX, thisBounds.Height));
                        }

                    }

                    if (curSample == PreSamples)
                        context.DrawLine(GraphicObjectsCache.GetPen(triggerLineColor, 2), new Point(lineX, 0), new Point(lineX, thisBounds.Height));

                    if (Bursts != null)
                    {
                        if (Bursts.Any(b => b == curSample))
                        {
                            context.DrawLine(GraphicObjectsCache.GetPen(burstLineColor, 2, DashStyle.DashDot), new Point(lineX, 0), new Point(lineX, thisBounds.Height));
                        }
                    }

                    if(UserMarker != null && UserMarker == curSample)
                        context.DrawLine(GraphicObjectsCache.GetPen(AnalyzerColors.UserLineColor, 2, DashStyle.DashDot), new Point(lineX, 0), new Point(lineX, thisBounds.Height));

                    if (curSample == FirstSample)
                    {
                        for (int chan = 0; chan < ChannelCount; chan++)
                        {
                            renders[chan].firstSample = curSample;
                            renders[chan].sampleCount = 1;
                            renders[chan].value = visibleChannels[chan].Samples[curSample];
                        }
                    }
                    else
                    {
                        for (int chan = 0; chan < ChannelCount; chan++)
                        {
                            if (renders[chan].value != visibleChannels[chan].Samples[curSample])
                            {
                                double yHi = chan * channelHeight + margin;
                                double yLo = yHi + channelHeight - margin * 2;

                                double xStart = (renders[chan].firstSample - FirstSample) * sampleWidth;
                                double xEnd = renders[chan].sampleCount * sampleWidth + xStart;

                                var pen = GraphicObjectsCache.GetPen(AnalyzerColors.GetChannelColor(visibleChannels[chan]), 2);

                                if (renders[chan].value != 0)
                                    context.DrawLine(pen, new Point(xStart, yHi), new Point(xEnd, yHi));
                                else
                                    context.DrawLine(pen, new Point(xStart, yLo), new Point(xEnd, yLo));


                                context.DrawLine(pen, new Point(xEnd, yHi), new Point(xEnd, yLo));

                                renders[chan].firstSample = curSample;
                                renders[chan].sampleCount = 1;
                                renders[chan].value = visibleChannels[chan].Samples[curSample];
                            }
                            else
                            {
                                renders[chan].sampleCount++;
                            }
                        }
                    }
                }

                for (int chan = 0; chan < ChannelCount; chan++)
                {
                    double yHi = chan * channelHeight + margin;
                    double yLo = yHi + channelHeight - margin * 2;

                    double xStart = (renders[chan].firstSample - FirstSample) * sampleWidth;
                    double xEnd = renders[chan].sampleCount * sampleWidth + xStart;

                    var pen = GraphicObjectsCache.GetPen(AnalyzerColors.GetChannelColor(visibleChannels[chan]), 2);

                    if (renders[chan].value != 0)
                        context.DrawLine(pen, new Point(xStart, yHi), new Point(xEnd, yHi));
                    else
                        context.DrawLine(pen, new Point(xStart, yLo), new Point(xEnd, yLo));
                }

                if (UserMarker != null && UserMarker == lastSample)
                {
                    double lineX = (lastSample - FirstSample) * sampleWidth;
                    context.DrawLine(GraphicObjectsCache.GetPen(AnalyzerColors.UserLineColor, 2, DashStyle.DashDot), new Point(lineX, 0), new Point(lineX, thisBounds.Height));
                }
            }
        }

        struct channelRenderStatus
        {
            public int firstSample;
            public int sampleCount;
            public byte value;
        }

        class interval
        {
            public bool value;
            public int start;
            public int end;
            public double duration;
        }
        
    }
}
