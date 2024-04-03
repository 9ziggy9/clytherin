FROM gcc:latest

WORKDIR /app

COPY . /app

RUN make all

CMD ["/app/build/run"]
