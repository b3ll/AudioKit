//
//  AKStereoDelayAudioUnit.swift
//  AudioKit
//
//  Created by Shane Dunne, revision history on Github.
//  Copyright © 2018 AudioKit. All rights reserved.
//

import AVFoundation

public class AKStereoDelayAudioUnit: AKAudioUnitBase {

    func setParameter(_ address: AKStereoDelayParameter, value: Double) {
        setParameterWithAddress(address.rawValue, value: Float(value))
    }

    func setParameterImmediately(_ address: AKStereoDelayParameter, value: Double) {
        setParameterImmediatelyWithAddress(address.rawValue, value: Float(value))
    }

    var time: Double = AKStereoDelay.defaultTime {
        didSet { setParameter(.time, value: time) }
    }

    var feedback: Double = AKStereoDelay.defaultFeedback {
        didSet { setParameter(.feedback, value: feedback) }
    }

    var dryWetMix: Double = AKStereoDelay.defaultDryWetMix {
        didSet { setParameter(.dryWetMix, value: dryWetMix) }
    }

    var pingPong: Bool = false {
        didSet { setParameter(.pingPong, value: pingPong ? 1.0 : 0.0) }
    }

    var rampDuration: Double = 0.0 {
        didSet { setParameter(.rampDuration, value: rampDuration) }
    }

    public override func initDSP(withSampleRate sampleRate: Double,
                                 channelCount count: AVAudioChannelCount) -> AKDSPRef {
        return createStereoDelayDSP(Int32(count), sampleRate)
    }

    public override init(componentDescription: AudioComponentDescription,
                                      options: AudioComponentInstantiationOptions = []) throws {
        try super.init(componentDescription: componentDescription, options: options)
        let time = AUParameterTree.createParameter(
            identifier: "time",
            name: "Delay time (Seconds)",
            address: AKStereoDelayParameter.time.rawValue,
            range: AKStereoDelay.timeRange,

            unit: .seconds,
            flags: .default)
        let feedback = AUParameterTree.createParameter(
            identifier: "feedback",
            name: "Feedback (%)",
            address: AKStereoDelayParameter.feedback.rawValue,
            range: AKStereoDelay.feedbackRange,

            unit: .generic,
            flags: .default)
        let dryWetMix = AUParameterTree.createParameter(
            identifier: "dryWetMix",
            name: "Dry-Wet Mix",
            address: AKStereoDelayParameter.dryWetMix.rawValue,
            range: AKStereoDelay.dryWetMixRange,

            unit: .generic,
            flags: .default)
        let pingPong = AUParameterTree.createParameter(
            identifier: "pingPong",
            name: "Ping-Pong Mode",
            address: AKStereoDelayParameter.pingPong.rawValue,
            min: Float(0.0),
            max: Float(1.0),
            unit: .boolean,
            flags: [.flag_IsReadable, .flag_IsWritable])


        setParameterTree(AUParameterTree(children: [time, feedback, dryWetMix, pingPong]))
        time.value = Float(AKStereoDelay.defaultTime)
        feedback.value = Float(AKStereoDelay.defaultFeedback)
        dryWetMix.value = Float(AKStereoDelay.defaultDryWetMix)
        pingPong.value = 0.0
    }

    public override var canProcessInPlace: Bool { return true }

}
