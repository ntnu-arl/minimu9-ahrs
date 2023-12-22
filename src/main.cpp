#include <stdio.h>
#include <signal.h>

#include "ros/ros.h"
#include "sensor_msgs/Imu.h"
#include "tf/transform_broadcaster.h"

void sig_handler(int s);
static bool got_ctrl_c = false;

///////////////////////////////////////////

// For efficiency in compiling, this is the only file that uses the
// Eigen lirbary and the 'vector' type we made from it.

#include "vector.h"
#include "version.h"
#include "prog_options.h"
#include "minimu9.h"
#include "exceptions.h"
#include "pacer.h"
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <system_error>
#include <chrono>

// TODO: print warning if accelerometer magnitude is not close to 1 when starting up

// An Euler angle could take 8 chars: -234.678, but usually we only need 6.
float field_width = 6;
#define FLOAT_FORMAT std::fixed << std::setprecision(3) << std::setw(field_width)

std::ostream &operator<<(std::ostream &os, const vector &vector)
{
    return os << FLOAT_FORMAT << vector(0) << ' '
              << FLOAT_FORMAT << vector(1) << ' '
              << FLOAT_FORMAT << vector(2);
}

std::ostream &operator<<(std::ostream &os, const matrix &matrix)
{
    return os << (vector)matrix.row(0) << ' '
              << (vector)matrix.row(1) << ' '
              << (vector)matrix.row(2);
}

std::ostream &operator<<(std::ostream &os, const quaternion &quat)
{
    return os << FLOAT_FORMAT << quat.w() << ' '
              << FLOAT_FORMAT << quat.x() << ' '
              << FLOAT_FORMAT << quat.y() << ' '
              << FLOAT_FORMAT << quat.z();
}

typedef void rotation_output_function(quaternion &rotation);

void output_quaternion(quaternion &rotation)
{
    // std::cout << rotation;
}

void output_matrix(quaternion &rotation)
{
    std::cout << rotation.toRotationMatrix();
}

void output_euler(quaternion &rotation)
{
    (vector)(rotation.toRotationMatrix().eulerAngles(2, 1, 0) * (180 / M_PI));
    // std::cout << (vector)(rotation.toRotationMatrix().eulerAngles(2, 1, 0) * (180 / M_PI));
}

void stream_raw_values(imu &imu)
{
    imu.enable();
    while (1)
    {
        imu.read_raw();
        printf("%7d %7d %7d  %7d %7d %7d  %7d %7d %7d\n",
               imu.m[0], imu.m[1], imu.m[2],
               imu.a[0], imu.a[1], imu.a[2],
               imu.g[0], imu.g[1], imu.g[2]);
        usleep(20 * 1000);
    }
}

//! Uses the acceleration and magnetic field readings from the compass
// to get a noisy estimate of the current rotation matrix.
// This function is where we define the coordinate system we are using
// for the ground coords:  North, East, Down.
/* JJ */
#if 0
/* 
  doesn't work...
  we want East North Up (ENU), but existing is NED... how to convert? 
*/
matrix rotation_from_compass(const vector & acceleration, const vector & magnetic_field)
{
  vector up = acceleration;

  vector east = up.cross(magnetic_field); // actually it's magnetic east
  vector north = east.cross(up);

  east.normalize();
  north.normalize();
  up.normalize();

  matrix r;
  r.row(0) = east;
  r.row(1) = north;
  r.row(2) = up;
  return r;
}
#elif 1
matrix rotation_from_compass(const vector &acceleration, const vector &magnetic_field)
{
    /* JJ -
     * Seemed to help (just this one change), only problem is x-axis rotation of robot sees change in z-axis
     * too (as presented in rviz). Which is the same problem we're seeing when no changes are made
     * (but the robot is also upside down in that case)
     */
    //  vector down = -acceleration;     // usually true
    vector down = acceleration; // usually true

    vector east = down.cross(magnetic_field); // actually it's magnetic east
    vector north = east.cross(down);

    east.normalize();
    north.normalize();
    down.normalize();

    matrix r;
    r.row(0) = north;
    r.row(1) = east;
    r.row(2) = down;
    return r;
}
#else
matrix rotation_from_compass(const vector &acceleration, const vector &magnetic_field)
{
    vector down = -acceleration; // usually true

    vector east = down.cross(magnetic_field); // actually it's magnetic east
    vector north = east.cross(down);

    east.normalize();
    north.normalize();
    down.normalize();

    matrix r;
    r.row(0) = north;
    r.row(1) = east;
    r.row(2) = down;
    return r;
}
#endif

typedef void fuse_function(quaternion &rotation, float dt, const vector &angular_velocity,
                           const vector &acceleration, const vector &magnetic_field);

void fuse_compass_only(quaternion &rotation, float dt, const vector &angular_velocity,
                       const vector &acceleration, const vector &magnetic_field)
{
    // Implicit conversion of rotation matrix to quaternion.
    rotation = rotation_from_compass(acceleration, magnetic_field);
}

// Uses the given angular velocity and time interval to calculate
// a rotation and applies that rotation to the given quaternion.
// w is angular velocity in radians per second.
// dt is the time.
void rotate(quaternion &rotation, const vector &w, float dt)
{
    // Multiply by first order approximation of the
    // quaternion representing this rotation.
    rotation *= quaternion(1, w(0) * dt / 2, w(1) * dt / 2, w(2) * dt / 2);
    rotation.normalize();
}

void fuse_gyro_only(quaternion &rotation, float dt, const vector &angular_velocity,
                    const vector &acceleration, const vector &magnetic_field)
{
    rotate(rotation, angular_velocity, dt);
}

void fuse_default(quaternion &rotation, float dt, const vector &angular_velocity,
                  const vector &acceleration, const vector &magnetic_field)
{
    vector correction = vector(0, 0, 0);

    if (fabs(acceleration.norm() - 1) <= 0.3)
    {
        // The magnetidude of acceleration is close to 1 g, so
        // it might be pointing up and we can do drift correction.

        const float correction_strength = 1;

        matrix rotation_compass = rotation_from_compass(acceleration, magnetic_field);
        matrix rotation_matrix = rotation.toRotationMatrix();

        correction = (rotation_compass.row(0).cross(rotation_matrix.row(0)) +
                      rotation_compass.row(1).cross(rotation_matrix.row(1)) +
                      rotation_compass.row(2).cross(rotation_matrix.row(2))) *
                     correction_strength;
    }

    rotate(rotation, angular_velocity + correction, dt);
}

/* JJ */
#if 1
// send tf as well as the orientation
void ahrs_ros(imu &imu, fuse_function *fuse, rotation_output_function *output)
{
    ros::NodeHandle node;
    static ros::Publisher imu_pub = node.advertise<sensor_msgs::Imu>("imu", 50);
    sensor_msgs::Imu imu_msg;
    tf::TransformBroadcaster odom_broadcaster;
    geometry_msgs::TransformStamped odom_trans;

    imu.load_calibration();
    imu.enable();
    imu.measure_offsets();

    // The quaternion that can convert a vector in body coordinates
    // to ground coordinates when it its changed to a matrix.
    quaternion rotation = quaternion::Identity();

    ros::NodeHandle private_node_handle_("~");
    ros::Rate loop_rate(1000); // this is what the author of the imu code used
    // ros::Rate loop_rate(10);

    auto start = std::chrono::steady_clock::now();
    while (ros::ok())
    {
        auto last_start = start;
        start = std::chrono::steady_clock::now();
        std::chrono::nanoseconds duration = start - last_start;
        float dt = duration.count() / 1e9;
        if (dt < 0)
        {
            throw std::runtime_error("Time went backwards.");
        }

        vector angular_velocity = imu.read_gyro();
        vector acceleration = imu.read_acc();
        vector magnetic_field = imu.read_mag();

// JJ - correct axis orientation to match ROS
#if 0
// do nothing
#elif 1

#if 0
    // these seemed to be verified correct... *** meaning do nothing! ***
    angular_velocity.x() = angular_velocity.x() * 1.0;
    angular_velocity.y() = angular_velocity.y() * 1.0;
    angular_velocity.z() = angular_velocity.z() * 1.0;
#elif 1
        angular_velocity.x() = 0;
        angular_velocity.y() = 0;
        // angular_velocity.z() = 0;
#endif

#if 0
    // can't seem to find any combination here that works
    acceleration.x() = acceleration.x() * -1.0;
    acceleration.y() = acceleration.y() * -1.0;
    acceleration.z() = acceleration.z() * -1.0;
#elif 0
        // zeroing these out seems to allow the gyro (and magnetometer?) to work correctly (ie. the robot stays right side up!)
        acceleration.x() = 0;
        acceleration.y() = 0;
        acceleration.z() = 0;
#endif

#if 0
    // do nothing seems to be correct here too?
    magnetic_field.x() = magnetic_field.x() * 1.0;
    magnetic_field.y() = magnetic_field.y() * 1.0;
    magnetic_field.z() = magnetic_field.z() * 1.0;
#elif 0
        magnetic_field.x() = 0;
        magnetic_field.y() = 0;
        magnetic_field.z() = 0;
#endif

#else
        angular_velocity.x() = angular_velocity.x() * 1.0;
        angular_velocity.y() = angular_velocity.y() * -1.0;
        angular_velocity.z() = angular_velocity.z() * -1.0;

        // already negated, so negate the negation!
        acceleration.x() = acceleration.x() * -1.0;
        acceleration.y() = acceleration.y() * 1.0;
        acceleration.z() = acceleration.z() * 1.0;

        magnetic_field.x() = magnetic_field.x() * 1.0;
        magnetic_field.y() = magnetic_field.y() * -1.0;
        magnetic_field.z() = magnetic_field.z() * -1.0;
#endif

        fuse(rotation, dt, angular_velocity, acceleration, magnetic_field);

        output(rotation);
        // std::cout << "  " << acceleration << "  " << magnetic_field << std::endl;

        // just publishes the orientation topic (might not be all that useful... no one is subscribing!)
        imu_msg.orientation.x = rotation.x();
        imu_msg.orientation.y = rotation.y();
        imu_msg.orientation.z = rotation.z();
        imu_msg.orientation.w = rotation.w();
        imu_msg.header.stamp = ros::Time::now();
        // imu_msg.header.frame_id = "base_footprint";
        imu_msg.header.frame_id = "base_imu";
        imu_pub.publish(imu_msg);

        // send the transform
        odom_trans.header.stamp = ros::Time::now();
        odom_trans.header.frame_id = "map";
        odom_trans.child_frame_id = "odom";
        odom_trans.transform.rotation.x = rotation.x();
        odom_trans.transform.rotation.y = rotation.y();
        odom_trans.transform.rotation.z = rotation.z();
        odom_trans.transform.rotation.w = rotation.w();
        odom_broadcaster.sendTransform(odom_trans);

        if (got_ctrl_c)
        {
            ROS_WARN("Got Ctrl-C");
            break;
        }

        ros::spinOnce();
        loop_rate.sleep();
    }
}
#elif 1
// just publishes the orientation topic (might not be all that useful)
void ahrs_ros(imu &imu, fuse_function *fuse, rotation_output_function *output)
{
    ros::NodeHandle node;
    static ros::Publisher imu_pub = node.advertise<sensor_msgs::Imu>("imu", 50);
    sensor_msgs::Imu imu_msg;

    imu.load_calibration();
    imu.enable();
    imu.measure_offsets();

    // The quaternion that can convert a vector in body coordinates
    // to ground coordinates when it its changed to a matrix.
    quaternion rotation = quaternion::Identity();

    ros::NodeHandle private_node_handle_("~");
    // ros::Rate loop_rate(50); // this is what the author of the imu code used
    ros::Rate loop_rate(10);

    auto start = std::chrono::steady_clock::now();
    while (ros::ok())
    {
        auto last_start = start;
        start = std::chrono::steady_clock::now();
        std::chrono::nanoseconds duration = start - last_start;
        float dt = duration.count() / 1e9;
        if (dt < 0)
        {
            throw std::runtime_error("Time went backwards.");
        }

        vector angular_velocity = imu.read_gyro();
        vector acceleration = imu.read_acc();
        vector magnetic_field = imu.read_mag();

        fuse(rotation, dt, angular_velocity, acceleration, magnetic_field);

        output(rotation);
        std::cout << "  " << acceleration << "  " << magnetic_field << std::endl;

        //    imu_msg.linear_acceleration.x = ;

        imu_msg.orientation.x = rotation.x();
        imu_msg.orientation.y = rotation.y();
        imu_msg.orientation.z = rotation.z();
        imu_msg.orientation.w = rotation.w();

        imu_msg.header.stamp = ros::Time::now();
        // imu_msg.header.frame_id = "base_footprint";
        imu_msg.header.frame_id = "base_imu";
        imu_pub.publish(imu_msg);

        if (got_ctrl_c)
        {
            ROS_WARN("Got Ctrl-C");
            break;
        }

        ros::spinOnce();
        loop_rate.sleep();
    }
}
#else
void ahrs_ros(imu &imu, fuse_function *fuse, rotation_output_function *output)
{
    imu.load_calibration();
    imu.enable();
    imu.measure_offsets();

    // The quaternion that can convert a vector in body coordinates
    // to ground coordinates when it its changed to a matrix.
    quaternion rotation = quaternion::Identity();

    ros::NodeHandle private_node_handle_("~");
    // ros::Rate loop_rate(50); // this is what the author of the imu code used
    ros::Rate loop_rate(20);

    auto start = std::chrono::steady_clock::now();
    while (ros::ok())
    {
        auto last_start = start;
        start = std::chrono::steady_clock::now();
        std::chrono::nanoseconds duration = start - last_start;
        float dt = duration.count() / 1e9;
        if (dt < 0)
        {
            throw std::runtime_error("Time went backwards.");
        }

        vector angular_velocity = imu.read_gyro();
        vector acceleration = imu.read_acc();
        vector magnetic_field = imu.read_mag();

        fuse(rotation, dt, angular_velocity, acceleration, magnetic_field);

        output(rotation);
        std::cout << "  " << acceleration << "  " << magnetic_field << std::endl;

        if (got_ctrl_c)
        {
            ROS_WARN("Got Ctrl-C");
            break;
        }

        ros::spinOnce();
        loop_rate.sleep();
    }
}
#endif

void ahrs(imu &imu, fuse_function *fuse, rotation_output_function *output)
{
    imu.load_calibration();
    imu.enable();
    imu.measure_offsets();

    // The quaternion that can convert a vector in body coordinates
    // to ground coordinates when it its changed to a matrix.
    quaternion rotation = quaternion::Identity();

    // Set up a timer that expires every 20 ms.
    pacer loop_pacer;
    loop_pacer.set_period_ns(20000000);

    auto start = std::chrono::steady_clock::now();
    while (1)
    {
        auto last_start = start;
        start = std::chrono::steady_clock::now();
        std::chrono::nanoseconds duration = start - last_start;
        float dt = duration.count() / 1e9;
        if (dt < 0)
        {
            throw std::runtime_error("Time went backwards.");
        }

        vector angular_velocity = imu.read_gyro();
        vector acceleration = imu.read_acc();
        vector magnetic_field = imu.read_mag();

        fuse(rotation, dt, angular_velocity, acceleration, magnetic_field);

        output(rotation);
        // std::cout << "  " << acceleration << "  " << magnetic_field << std::endl;

        loop_pacer.pace();
    }
}

int main_with_exceptions(int argc, char **argv)
{
    // Decide what sensors we want to use.
    sensor_set set;
    set.mag = set.acc = set.gyro = true;

    minimu9::comm_config config = minimu9::auto_detect("/dev/i2c-1");

    sensor_set missing = set - minimu9::config_sensor_set(config);
    if (missing)
    {
        if (missing.mag)
        {
            std::cerr << "Error: No magnetometer found." << std::endl;
        }
        if (missing.acc)
        {
            std::cerr << "Error: No accelerometer found." << std::endl;
        }
        if (missing.gyro)
        {
            std::cerr << "Error: No gyro found." << std::endl;
        }
        std::cerr << "Error: Needed sensors are missing." << std::endl;
        return 1;
    }

    config = minimu9::disable_redundant_sensors(config, set);

    minimu9::handle imu;
    imu.open(config);

    rotation_output_function *output;

/* JJ */
#if 1
    field_width += 2; // See comment above for field_width.
    output = &output_euler;

    // fuse_gyro_only: gyro only works fine, with no changes!
    // ahrs_ros(imu, &fuse_gyro_only, output);

    // fuse_compass_only: has problems... robot upside down, x-axis rotation sees rotation in z-axis too
    // ahrs_ros(imu, &fuse_compass_only, output);

    // fuse_default: has problems... robot upside down, x-axis rotation sees rotation in z-axis too
    ahrs_ros(imu, &fuse_default, output);
#else
    // Figure out the output mode.
    if (options.output_mode == "matrix")
    {
        output = &output_matrix;
    }
    else if (options.output_mode == "quaternion")
    {
        output = &output_quaternion;
    }
    else if (options.output_mode == "euler")
    {
        field_width += 2; // See comment above for field_width.
        output = &output_euler;
    }
    else
    {
        std::cerr << "Unknown output mode '" << options.output_mode << "'" << std::endl;
        return 1;
    }

    // Figure out the basic operating mode and start running.
    if (options.mode == "raw")
    {
        stream_raw_values(imu);
    }
    else if (options.mode == "gyro-only")
    {
        ahrs(imu, &fuse_gyro_only, output);
    }
    else if (options.mode == "compass-only")
    {
        ahrs(imu, &fuse_compass_only, output);
    }
    else if (options.mode == "normal")
    {
        ahrs(imu, &fuse_default, output);
    }
    else
    {
        std::cerr << "Unknown mode '" << options.mode << "'" << std::endl;
        return 1;
    }
#endif

    return 0;
}

///////////////////////////////////////////

int main(int argc, char *argv[])
{
    printf("Hello from minimu9-ahrs!\n");
    signal(SIGINT, sig_handler);
    ros::init(argc, argv, "minimu9");

#if 1
    main_with_exceptions(argc, argv);
#else

    ros::NodeHandle private_node_handle_("~");
    ros::Rate loop_rate(10);

    while (ros::ok())
    {
        if (got_ctrl_c)
        {
            ROS_WARN("Got Ctrl-C");
            break;
        }

        ros::spinOnce();
        loop_rate.sleep();
    }
#endif

    ROS_WARN("Exiting.");

    exit(0);
}

void sig_handler(int s)
{
    if (s == 2)
    {
        got_ctrl_c = true;
    }
}
