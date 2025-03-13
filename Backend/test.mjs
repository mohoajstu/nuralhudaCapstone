// test/processAudio.test.js
import request from "supertest";
import fs from "fs";
import path from "path";
import { expect } from "chai";
import app from "./server.mjs"; // Adjust the path as needed

describe("POST /process-audio endpoint", function () {
  this.timeout(10000);

  it("should process an MP3 file, return a chunked MP3 response, and save output to output.mp3", function (done) {
    const inputFilePath = path.join("input.wav");
    const outputFilePath = path.join("output.mp3");

    // Read the input MP3 file from disk
    fs.readFile(inputFilePath, (err, data) => {
      if (err) return done(err);

      request(app)
        .post("/process-audio")
        .set("Content-Type", "audio/wav")
        .send(data)
        // Buffer the response so we can work with it as a Buffer
        .buffer()
        .parse((res, callback) => {
          const chunks = [];
          res.on("data", (chunk) => chunks.push(chunk));
          res.on("end", () => callback(null, Buffer.concat(chunks)));
        })
        .expect("Content-Type", /audio\/mpeg/)
        .expect(200)
        .end((err, res) => {
          if (err) return done(err);
          // Verify that the response is a non-empty Buffer
          expect(res.body).to.be.instanceof(Buffer);
          expect(res.body.length).to.be.greaterThan(0);

          // Save the received MP3 response to output.mp3
          fs.writeFile(outputFilePath, res.body, (err) => {
            if (err) return done(err);
            // Optionally, verify the file was written correctly by checking its size
            const outputStats = fs.statSync(outputFilePath);
            expect(outputStats.size).to.equal(res.body.length);
            done();
          });
        });
    });
  });
});
