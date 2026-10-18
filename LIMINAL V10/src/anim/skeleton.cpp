#include "anim/skeleton.hpp"

#include <algorithm>
#include <cmath>

namespace lim::anim {

const std::array<Bone, kBoneCount> kBoneParent = {
    B_ROOT,   B_ROOT,   B_PELVIS, B_SPINE,  B_CHEST,  B_NECK,   B_HEAD,
    B_CHEST,  B_CLAV_L, B_UARM_L, B_FARM_L, B_HAND_L, B_HAND_L,
    B_CHEST,  B_CLAV_R, B_UARM_R, B_FARM_R, B_HAND_R, B_HAND_R,
    B_PELVIS, B_THIGH_L, B_SHIN_L, B_FOOT_L,
    B_PELVIS, B_THIGH_R, B_SHIN_R, B_FOOT_R,
};

const std::array<BoneSeg, kBoneCount> kBoneSeg = {{
    {J_ROOT, J_PELVIS},
    {J_PELVIS, J_SPINE},
    {J_SPINE, J_CHEST},
    {J_CHEST, J_NECK},
    {J_NECK, J_HEAD},
    {J_HEAD, J_HEADTOP},
    {J_JAW, J_CHIN},
    {J_STERN_L, J_SHOULDER_L}, {J_SHOULDER_L, J_ELBOW_L}, {J_ELBOW_L, J_WRIST_L}, {J_WRIST_L, J_KNUCKLE_L},
    {J_KNUCKLE_L, J_FINGERTIP_L}, {J_THUMBBASE_L, J_THUMBTIP_L},
    {J_STERN_R, J_SHOULDER_R}, {J_SHOULDER_R, J_ELBOW_R}, {J_ELBOW_R, J_WRIST_R}, {J_WRIST_R, J_KNUCKLE_R},
    {J_KNUCKLE_R, J_FINGERTIP_R}, {J_THUMBBASE_R, J_THUMBTIP_R},
    {J_HIP_L, J_KNEE_L}, {J_KNEE_L, J_ANKLE_L}, {J_ANKLE_L, J_BALL_L}, {J_BALL_L, J_TOETIP_L},
    {J_HIP_R, J_KNEE_R}, {J_KNEE_R, J_ANKLE_R}, {J_ANKLE_R, J_BALL_R}, {J_BALL_R, J_TOETIP_R},
}};

const char* boneName(Bone b) {
    static const char* names[kBoneCount] = {"root",   "pelvis", "spine",  "chest",   "neck",   "head",    "jaw",
                                            "clav_l", "uarm_l", "farm_l", "hand_l",  "fing_l", "thumb_l",
                                            "clav_r", "uarm_r", "farm_r", "hand_r",  "fing_r", "thumb_r",
                                            "thigh_l", "shin_l", "foot_l", "toe_l", "thigh_r", "shin_r", "foot_r", "toe_r"};
    return names[b];
}

void Build::finish() {
    const float H = height;
    if (ankle <= 0) ankle = 0.040f * H;
    if (shin <= 0) shin = 0.245f * H;
    if (thigh <= 0) thigh = 0.232f * H;
    if (hipHeight <= 0) hipHeight = ankle + shin + thigh;
    if (foot <= 0) foot = 0.108f * H;
    // Rumpf: was zwischen Becken und Kopf bleibt (Kopf ~0,27 m ueber dem Halsansatz, siehe bindPose)
    {
        const float hs = headScale * (0.6f + 0.4f * H / 1.78f);
        const float rem = H - 0.27f * hs - (hipHeight + 0.018f * H);
        const float share[3] = {0.30f, 0.38f, 0.32f};
        float* v[3] = {&spineLow, &spineUp, &neck};
        float setSum = 0.0f, freeShare = 0.0f;
        for (int i = 0; i < 3; ++i) {
            if (*v[i] > 0) setSum += *v[i];
            else freeShare += share[i];
        }
        for (int i = 0; i < 3; ++i)
            if (*v[i] <= 0) *v[i] = std::max(0.04f, (rem - setSum) * share[i] / std::max(freeShare, 1e-3f));
    }
    if (headH <= 0) headH = 0.132f * H * headScale;
    if (upperArm <= 0) upperArm = 0.172f * H;
    if (forearm <= 0) forearm = 0.146f * H;
    if (hand <= 0) hand = 0.104f * H;
    if (shoulderW <= 0) shoulderW = 0.198f * H;
    if (hipW <= 0) hipW = 0.094f * H;
}

void ik2(vec3 A, vec3 T, float l1, float l2, vec3 pole, vec3& mid, vec3& end) {
    vec3 d = T - A;
    float dist = length(d);
    vec3 dir = dist > 1e-5f ? d / dist : vec3(0, 0, -1);
    dist = std::clamp(dist, std::fabs(l1 - l2) + 1e-3f, l1 + l2 - 1e-4f);
    float a = (l1 * l1 - l2 * l2 + dist * dist) / (2.0f * dist);
    float h = std::sqrt(std::max(0.0f, l1 * l1 - a * a));
    vec3 pp = orthoTo(pole, dir);
    float pl = length(pp);
    if (pl < 1e-4f) {
        pp = cross(dir, vec3(0, 0, 1));
        pl = length(pp);
        if (pl < 1e-4f) pp = vec3(1, 0, 0), pl = 1.0f;
    }
    pp = pp / pl;
    mid = A + dir * a + pp * h;
    end = A + dir * dist;
}

void placeHand(JointPose& p, const Build& b, int s, vec3 handDir) {
    const vec3 W = p.j[sideJoint(J_WRIST_L, s)];
    vec3 d = normalize(handDir);
    vec3 palm = normalize(orthoTo(p.palm[s], d));
    vec3 side = normalize(cross(palm, d));  // Richtung Daumen (rechte Hand: nach links gespiegelt)
    if (s == 1) side = side * -1.0f;
    const float palmLen = b.hand * 0.45f, fingLen = b.hand * 0.55f * b.fingerLen;
    vec3 K = W + d * palmLen;
    // Krummen: die Finger drehen um die Achse quer zur Hand Richtung Handflaeche
    vec3 axis = normalize(cross(d, palm));
    float curl = std::clamp(p.fingerCurl[s], -0.2f, 1.2f) * 1.9f;
    vec3 fd = rotateAround(d, axis, -curl);
    if (dot(fd, palm) < dot(d, palm) - 1e-3f && curl > 0.0f) fd = rotateAround(d, axis, curl);  // Richtung absichern
    p.j[sideJoint(J_KNUCKLE_L, s)] = K;
    p.j[sideJoint(J_FINGERTIP_L, s)] = K + fd * fingLen;
    vec3 TB = W + d * (palmLen * 0.25f) + side * (b.handW * 0.85f) + palm * 0.008f;
    float tc = std::clamp(p.thumbCurl[s], 0.0f, 1.0f);
    vec3 td = normalize(d * (0.75f - 0.35f * tc) + side * (0.55f - 0.45f * tc) + palm * (0.25f + 0.55f * tc));
    p.j[sideJoint(J_THUMBBASE_L, s)] = TB;
    p.j[sideJoint(J_THUMBTIP_L, s)] = TB + td * (b.hand * 0.42f * b.fingerLen);
}

void placeFoot(JointPose& p, const Build& b, int s, vec3 footFwd) {
    const vec3 A = p.j[legJoint(J_ANKLE_L, s)];
    vec3 up = normalize(p.footUp[s]);
    vec3 f = normalize(orthoTo(footFwd, up));
    // Ballen liegt vor und unter dem Knoechel
    vec3 ball = A + f * (b.foot * 0.72f) - up * (b.ankle * 0.82f);
    p.j[legJoint(J_BALL_L, s)] = ball;
    p.j[legJoint(J_TOETIP_L, s)] = ball + f * (b.foot * 0.28f);
}

JointPose bindPose(const Build& bIn) {
    Build b = bIn;
    b.finish();
    JointPose p;
    const vec3 F{1, 0, 0}, L{0, 1, 0}, U{0, 0, 1};  // links = +y (Blick +x, z oben)
    p.fwd = F, p.up = U, p.chestFwd = F, p.headFwd = F, p.headUp = U;
    const float pelvisZ = b.hipHeight + 0.018f * b.height;
    p.j[J_ROOT] = {0, 0, 0};
    p.j[J_PELVIS] = U * pelvisZ;
    p.j[J_SPINE] = p.j[J_PELVIS] + U * b.spineLow - F * 0.012f * b.height;
    p.j[J_CHEST] = p.j[J_SPINE] + U * b.spineUp + F * 0.004f * b.height;
    p.j[J_NECK] = p.j[J_CHEST] + U * b.neck - F * 0.012f * b.height;
    // Kopf: Masse einer Norm-Figur von 1,78 m, mit headScale skaliert (der Kopf waechst nicht mit der Groesse)
    const float hs = b.headScale * (0.6f + 0.4f * b.height / 1.78f);
    p.j[J_HEAD] = p.j[J_NECK] + U * (0.11f * hs) + F * (0.032f * hs);
    p.j[J_HEADTOP] = p.j[J_HEAD] + U * (0.16f * hs) - F * (0.009f * hs);
    p.j[J_JAW] = p.j[J_HEAD] + F * (0.032f * hs) + U * (0.007f * hs);
    p.j[J_CHIN] = p.j[J_HEAD] + F * (0.107f * hs) - U * (0.075f * hs);
    for (int s = 0; s < 2; ++s) {
        const float sg = s == 0 ? 1.0f : -1.0f;  // links +y
        vec3 st = p.j[J_NECK] - U * (0.035f * b.height) + F * (0.035f * b.height) + L * (sg * 0.012f * b.height);
        vec3 sh = p.j[J_NECK] - U * (0.022f * b.height + b.shoulderDrop) + L * (sg * b.shoulderW * 0.5f) - F * 0.008f * b.height;
        p.j[sideJoint(J_STERN_L, s)] = st;
        p.j[sideJoint(J_SHOULDER_L, s)] = sh;
        // Arm haengt seitlich ab (armAbduct), Ellbogen leicht gebeugt, Handflaeche zum Oberschenkel
        vec3 down = normalize(U * -std::cos(b.armAbduct) + L * (sg * std::sin(b.armAbduct)));
        vec3 elbow = sh + down * b.upperArm;
        vec3 fdir = normalize(down + F * 0.18f);
        vec3 wrist = elbow + fdir * b.forearm;
        p.j[sideJoint(J_ELBOW_L, s)] = elbow;
        p.j[sideJoint(J_WRIST_L, s)] = wrist;
        p.armPole[s] = normalize(F * -1.0f + L * (sg * 0.15f));
        vec3 outward = normalize(L * sg - down * dot(L * sg, down));
        p.palm[s] = outward * -1.0f;  // zur Koerpermitte
        p.fingerCurl[s] = 0.15f;
        p.thumbCurl[s] = 0.25f;
        placeHand(p, b, s, fdir);
        // Beine: leicht gespreizt
        vec3 hip = p.j[J_PELVIS] + L * (sg * b.hipW * 0.5f) - U * (0.018f * b.height);
        vec3 ankle = vec3{0.0f, sg * (b.hipW * 0.5f + 0.022f * b.height), b.ankle};
        vec3 knee, an;
        ik2(hip, ankle, b.thigh, b.shin, F, knee, an);
        p.j[legJoint(J_HIP_L, s)] = hip;
        p.j[legJoint(J_KNEE_L, s)] = knee;
        p.j[legJoint(J_ANKLE_L, s)] = an;
        p.legPole[s] = F;
        p.footUp[s] = U;
        placeFoot(p, b, s, normalize(F + L * (sg * 0.12f)));
    }
    return p;
}

namespace {
mat4 frame(vec3 a, vec3 b, vec3 hint) {
    vec3 x = b - a;
    float len = length(x);
    x = len > 1e-6f ? x / len : vec3(0, 0, 1);
    vec3 z = orthoTo(hint, x);
    if (length(z) < 1e-4f) z = orthoTo(std::fabs(x.z) < 0.9f ? vec3(0, 0, 1) : vec3(1, 0, 0), x);
    z = normalize(z);
    vec3 y = cross(z, x);
    mat4 m;
    m.c[0] = vec4(x, 0);
    m.c[1] = vec4(y, 0);
    m.c[2] = vec4(z, 0);
    m.c[3] = vec4(a, 1);
    return m;
}
}  // namespace

void computeFrames(const JointPose& p, Frames& f) {
    const auto& j = p.j;
    f[B_ROOT] = frame(j[J_ROOT], j[J_ROOT] + p.fwd, p.up);
    f[B_PELVIS] = frame(j[J_PELVIS], j[J_SPINE], p.fwd);
    f[B_SPINE] = frame(j[J_SPINE], j[J_CHEST], normalize(p.fwd + p.chestFwd));
    f[B_CHEST] = frame(j[J_CHEST], j[J_NECK], p.chestFwd);
    f[B_NECK] = frame(j[J_NECK], j[J_HEAD], normalize(p.chestFwd + p.headFwd));
    f[B_HEAD] = frame(j[J_HEAD], j[J_HEAD] + p.headUp, p.headFwd);
    // Kiefer: um die Querachse geoeffnet
    {
        vec3 chin = j[J_CHIN];
        if (p.jawOpen > 0.0f) {
            vec3 axis = normalize(cross(p.headUp, p.headFwd));
            chin = j[J_JAW] + rotateAround(chin - j[J_JAW], axis, p.jawOpen * 0.35f);
        }
        f[B_JAW] = frame(j[J_JAW], chin, p.headUp);
    }
    for (int s = 0; s < 2; ++s) {
        f[sideBone(B_CLAV_L, s)] = frame(j[sideJoint(J_STERN_L, s)], j[sideJoint(J_SHOULDER_L, s)], p.up);
        f[sideBone(B_UARM_L, s)] = frame(j[sideJoint(J_SHOULDER_L, s)], j[sideJoint(J_ELBOW_L, s)], p.armPole[s]);
        f[sideBone(B_FARM_L, s)] = frame(j[sideJoint(J_ELBOW_L, s)], j[sideJoint(J_WRIST_L, s)], p.armPole[s]);
        f[sideBone(B_HAND_L, s)] = frame(j[sideJoint(J_WRIST_L, s)], j[sideJoint(J_KNUCKLE_L, s)], p.palm[s]);
        f[sideBone(B_FING_L, s)] = frame(j[sideJoint(J_KNUCKLE_L, s)], j[sideJoint(J_FINGERTIP_L, s)], p.palm[s]);
        f[sideBone(B_THUMB_L, s)] = frame(j[sideJoint(J_THUMBBASE_L, s)], j[sideJoint(J_THUMBTIP_L, s)], p.palm[s]);
        f[legBone(B_THIGH_L, s)] = frame(j[legJoint(J_HIP_L, s)], j[legJoint(J_KNEE_L, s)], p.legPole[s]);
        f[legBone(B_SHIN_L, s)] = frame(j[legJoint(J_KNEE_L, s)], j[legJoint(J_ANKLE_L, s)], p.legPole[s]);
        f[legBone(B_FOOT_L, s)] = frame(j[legJoint(J_ANKLE_L, s)], j[legJoint(J_BALL_L, s)], p.footUp[s]);
        f[legBone(B_TOE_L, s)] = frame(j[legJoint(J_BALL_L, s)], j[legJoint(J_TOETIP_L, s)], p.footUp[s]);
    }
}

mat4 rigidInverse(const mat4& m) {
    vec3 x = m.c[0].xyz(), y = m.c[1].xyz(), z = m.c[2].xyz(), t = m.c[3].xyz();
    return mat4::fromRows({x, -dot(x, t)}, {y, -dot(y, t)}, {z, -dot(z, t)}, {0, 0, 0, 1});
}

void skinMatrices(const Frames& anim, const Frames& bindInv, vec3 origin, Frames& out) {
    const mat4 back = mat4::translation(vec3{0, 0, 0} - origin);
    for (int b = 0; b < kBoneCount; ++b) out[(size_t)b] = back * anim[(size_t)b] * bindInv[(size_t)b];
}

}  // namespace lim::anim
