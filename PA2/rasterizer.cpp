// clang-format off
//
// Created by goksu on 4/6/19.
//

#include <algorithm>
#include <vector>
#include "rasterizer.hpp"
#include <opencv2/opencv.hpp>
#include <math.h>


rst::pos_buf_id rst::rasterizer::load_positions(const std::vector<Eigen::Vector3f> &positions)
{
    auto id = get_next_id();
    pos_buf.emplace(id, positions);

    return {id};
}

rst::ind_buf_id rst::rasterizer::load_indices(const std::vector<Eigen::Vector3i> &indices)
{
    auto id = get_next_id();
    ind_buf.emplace(id, indices);

    return {id};
}

rst::col_buf_id rst::rasterizer::load_colors(const std::vector<Eigen::Vector3f> &cols)
{
    auto id = get_next_id();
    col_buf.emplace(id, cols);

    return {id};
}

auto to_vec4(const Eigen::Vector3f& v3, float w = 1.0f)
{
    return Vector4f(v3.x(), v3.y(), v3.z(), w);
}

rst::PN getPN(float val)
{
    return val >= 0.0 ? rst::PN::Pos : rst::PN::Neg;
}

static bool insideTriangle(int x, int y, const Vector3f* _v)
{   
    // TODO : Implement this function to check if the point (x, y) is inside the triangle represented by _v[0], _v[1], _v[2]
    Eigen::Vector3f ab(_v[1].x() - _v[0].x(), _v[1].y() - _v[0].y(), _v[1].z() - _v[0].z());  //向量ab
    Eigen::Vector3f bc(_v[2].x() - _v[1].x(), _v[2].y() - _v[1].y(), _v[2].z() - _v[1].z());  //向量bc
    Eigen::Vector3f ca(_v[0].x() - _v[2].x(), _v[0].y() - _v[2].y(), _v[0].z() - _v[2].z());  //向量ca

    Eigen::Vector3f ap(x - _v[0].x(),y -  _v[0].y(), 0.f);  //向量ap
    Eigen::Vector3f bp(x - _v[1].x(), y - _v[1].y(), 0.f);  //向量bp
    Eigen::Vector3f cp(x - _v[2].x(), y - _v[2].y(), 0.f);  //向量cp

    Eigen::Vector3f AP_cross_AB = ap.cross(ab);  //向量ab,ap叉乘
    Eigen::Vector3f BP_cross_BC = bp.cross(bc);  //向量ab,ap叉乘
    Eigen::Vector3f CP_cross_CA = cp.cross(ca);  //向量ab,ap叉乘

    return ((AP_cross_AB.z() < 0) && (BP_cross_BC.z() < 0) && (CP_cross_CA.z() < 0)) ? true : false; //不考虑点在边上的情况
   /* Vector3f p0pQ = { x - v[0].x(), y - v[0].y(), 0.0 };
    Vector3f p1pQ = { x - v[1].x(), y - v[1].y(), 0.0 };
    Vector3f p2pQ = { x - v[2].x(), y - v[2].y(), 0.0 };

    Vector3f p0p1 = { v[0].x() - v[1].x(), v[0].y() - v[1].y(), 0.0 };
    Vector3f p1p2 = { v[1].x() - v[2].x(), v[1].y() - v[2].y(), 0.0 };
    Vector3f p2p0 = { v[2].x() - v[0].x(), v[2].y() - v[0].y(), 0.0 };

    Vector3f p0p1_cross_p0pQ = p0p1.cross(p0pQ);
    Vector3f p1p2_cross_p1pQ = p1p2.cross(p1pQ);
    Vector3f p2p0_cross_p2pQ = p2p0.cross(p2pQ);

    return getPN(p0p1_cross_p0pQ.z()) == getPN(p1p2_cross_p1pQ.z()) &&
        getPN(p1p2_cross_p1pQ.z()) == getPN(p2p0_cross_p2pQ.z()) &&
        getPN(p2p0_cross_p2pQ.z()) == getPN(p0p1_cross_p0pQ.z()) ? true : false;*/
}

static std::tuple<float, float, float> computeBarycentric2D(float x, float y, const Vector3f* v)
{
    float c1 = (x*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*y + v[1].x()*v[2].y() - v[2].x()*v[1].y()) / (v[0].x()*(v[1].y() - v[2].y()) + (v[2].x() - v[1].x())*v[0].y() + v[1].x()*v[2].y() - v[2].x()*v[1].y());
    float c2 = (x*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*y + v[2].x()*v[0].y() - v[0].x()*v[2].y()) / (v[1].x()*(v[2].y() - v[0].y()) + (v[0].x() - v[2].x())*v[1].y() + v[2].x()*v[0].y() - v[0].x()*v[2].y());
    float c3 = (x*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*y + v[0].x()*v[1].y() - v[1].x()*v[0].y()) / (v[2].x()*(v[0].y() - v[1].y()) + (v[1].x() - v[0].x())*v[2].y() + v[0].x()*v[1].y() - v[1].x()*v[0].y());
    return {c1,c2,c3}; 
}

void rst::rasterizer::draw(pos_buf_id pos_buffer, ind_buf_id ind_buffer, col_buf_id col_buffer, Primitive type)
{
    auto& buf = pos_buf[pos_buffer.pos_id];
    auto& ind = ind_buf[ind_buffer.ind_id];
    auto& col = col_buf[col_buffer.col_id];

    float f1 = (50 - 0.1) / 2.0;
    float f2 = (50 + 0.1) / 2.0;

    Eigen::Matrix4f mvp = projection * view * model;
    for (auto& i : ind)
    {
        Triangle t;
        Eigen::Vector4f v[] = {
                mvp * to_vec4(buf[i[0]], 1.0f),
                mvp * to_vec4(buf[i[1]], 1.0f),
                mvp * to_vec4(buf[i[2]], 1.0f)
        };
        //Homogeneous division
        for (auto& vec : v) {
            vec /= vec.w();
        }
        //Viewport transformation 像素中心加0.5
        for (auto & vert : v)
        {
            vert.x() = 0.5*width*(vert.x()+1.0);
            vert.y() = 0.5*height*(vert.y()+1.0);
            vert.z() = vert.z() * f1 + f2;
        }

        for (int i = 0; i < 3; ++i)
        {
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
            t.setVertex(i, v[i].head<3>());
        }

        auto col_x = col[i[0]];
        auto col_y = col[i[1]];
        auto col_z = col[i[2]];

        t.setColor(0, col_x[0], col_x[1], col_x[2]);
        t.setColor(1, col_y[0], col_y[1], col_y[2]);
        t.setColor(2, col_z[0], col_z[1], col_z[2]);

        rasterize_triangle(t);
    }
}

//Screen space rasterization
void rst::rasterizer::rasterize_triangle(const Triangle& t) {
    auto v = t.toVector4();

    Eigen::Vector3f current_pixel (0.f, 0.f, 0.f); //当前像素点
    for (; current_pixel.x() < width; ++current_pixel.x())
    {
        for (; current_pixel.y() < height; ++current_pixel.y())
        {
            if (insideTriangle(current_pixel.x() + 0.5, current_pixel.y() + 0.5, t.v))
            {
                float alpha, beta, gamma;
                std::tie(alpha, beta, gamma) = computeBarycentric2D(current_pixel.x(), current_pixel.y(), t.v);
                float w_reciprocal = 1.0 / (alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
                float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
                z_interpolated *= w_reciprocal;

                int index = get_index(current_pixel.x(), current_pixel.y());
                depth_buf[index] = z_interpolated > depth_buf[index] ? depth_buf[index] : z_interpolated;
                set_pixel(current_pixel, t.getColor());
            }
        }
    }

    // TODO : Find out the bounding box of current triangle.
    // iterate through the pixel and find if the current pixel is inside the triangle

    // If so, use the following code to get the interpolated z value.
    //auto[alpha, beta, gamma] = computeBarycentric2D(x, y, t.v);
    //float w_reciprocal = 1.0/(alpha / v[0].w() + beta / v[1].w() + gamma / v[2].w());
    //float z_interpolated = alpha * v[0].z() / v[0].w() + beta * v[1].z() / v[1].w() + gamma * v[2].z() / v[2].w();
    //z_interpolated *= w_reciprocal;

    // TODO : set the current pixel (use the set_pixel function) to the color of the triangle (use getColor function) if it should be painted.
}

void rst::rasterizer::set_model(const Eigen::Matrix4f& m)
{
    model = m;
}

void rst::rasterizer::set_view(const Eigen::Matrix4f& v)
{
    view = v;
}

void rst::rasterizer::set_projection(const Eigen::Matrix4f& p)
{
    projection = p;
}

void rst::rasterizer::clear(rst::Buffers buff)
{
    if ((buff & rst::Buffers::Color) == rst::Buffers::Color)
    {
        std::fill(frame_buf.begin(), frame_buf.end(), Eigen::Vector3f{0, 0, 0});
    }
    if ((buff & rst::Buffers::Depth) == rst::Buffers::Depth)
    {
        std::fill(depth_buf.begin(), depth_buf.end(), std::numeric_limits<float>::infinity());
    }
}

rst::rasterizer::rasterizer(int w, int h) : width(w), height(h)
{
    frame_buf.resize(w * h);
    depth_buf.resize(w * h);
}

int rst::rasterizer::get_index(int x, int y)
{
    return (height-1-y)*width + x;
}

void rst::rasterizer::set_pixel(const Eigen::Vector3f& point, const Eigen::Vector3f& color)
{
    //old index: auto ind = point.y() + point.x() * width;
    auto ind = (height-1-point.y())*width + point.x();
    frame_buf[ind] = color;

}

// clang-format on