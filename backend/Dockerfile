FROM ai-task-estimator-backend-base:latest

WORKDIR /app

COPY backend/CMakeLists.txt /app/CMakeLists.txt
COPY backend/include /app/include
COPY backend/src /app/src
COPY backend/crow_all.h /app/crow_all.h
COPY training /app/training

RUN cmake -S /app -B /app/build && cmake --build /app/build -j$(nproc)

EXPOSE 8080
CMD ["/app/build/task_estimator_backend"]