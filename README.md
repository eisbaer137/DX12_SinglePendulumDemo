# DX12_SinglePendulumDemo
Simple pendulum demonstration using WIN API32 with DirectX 12

Simple pendulum's motion is governed by Newtonian equation of motion. Its nonlinear differential equation is sampled uniformly in time to give the difference equation. Using the difference equation with the well-known Euler method, the pendulum's location is computed at each moment. Stencil buffer was used to emulate the image of the pendulum being appeared in the mirror.

Elevation and Azimuthal angles can be changed by dragging your mouse while keeping pressing on the left mouse button. azimuthal and elevation angles are restricted so that you can only see the front side of the scene. You can zoom the scene in or out by dragging your mouse while keeping your right mouse button being pressed down.

A modeless type dialog box is attached at the right top of the window, where you can initialize the pendulum's initial angle with respect to a imaginary vertical line. After you input a value and click 'APPLY' button, you need to activate the main window by clicking mouse or whatever to see the pendulum's motion.

This demo is built using Microsoft Visual Studio 2022 community version on Windows 10 home.

Test system: intel core i7-7700 with nVidia GeForce RTX 3050

![singlePendulum](https://github.com/eisbaer137/DX12_SinglePendulumDemo/assets/166890279/7522b80d-bfd3-4056-a18f-d987ef19cdc4)
